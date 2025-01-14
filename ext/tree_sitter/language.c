#include "tree_sitter.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

typedef TSLanguage *(tree_sitter_lang)(void);
const char *tree_sitter_prefix = "tree_sitter_";

extern VALUE mTreeSitter;

VALUE cLanguage;

DATA_TYPE(TSLanguage *, language)
DATA_FREE(language)
DATA_MEMSIZE(language)
DATA_DECLARE_DATA_TYPE(language)
DATA_ALLOCATE(language)
DATA_UNWRAP(language)

TSLanguage *value_to_language(VALUE self) { return SELF; }

VALUE new_language(const TSLanguage *language) {
  VALUE res = language_allocate(cLanguage);
  unwrap(res)->data = (TSLanguage *)language;
  return res;
}

static VALUE language_symbol_count(VALUE self) {
  return UINT2NUM(ts_language_symbol_count(SELF));
}

static VALUE language_symbol_name(VALUE self, VALUE symbol) {
  return safe_str(ts_language_symbol_name(SELF, NUM2UINT(symbol)));
}

static VALUE language_symbol_for_name(VALUE self, VALUE string,
                                      VALUE is_named) {
  const char *str = rb_id2name(SYM2ID(string));
  uint32_t length = (uint32_t)strlen(str);
  bool named = RTEST(is_named);
  return UINT2NUM(ts_language_symbol_for_name(SELF, str, length, named));
}

static VALUE language_field_count(VALUE self) {
  return UINT2NUM(ts_language_field_count(SELF));
}

static VALUE language_field_name_for_id(VALUE self, VALUE field_id) {
  return safe_str(ts_language_field_name_for_id(SELF, NUM2UINT(field_id)));
}

static VALUE language_field_id_for_name(VALUE self, VALUE name) {
  TSLanguage *language = SELF;
  const char *str = StringValuePtr(name);
  uint32_t length = (uint32_t)RSTRING_LEN(name);
  return UINT2NUM(ts_language_field_id_for_name(language, str, length));
}

static VALUE language_symbol_type(VALUE self, VALUE symbol) {
  return new_symbol_type(ts_language_symbol_type(SELF, NUM2UINT(symbol)));
}

static VALUE language_version(VALUE self) {
  return UINT2NUM(ts_language_version(SELF));
}

static VALUE language_load(VALUE self, VALUE name, VALUE path) {
  VALUE path_s = rb_funcall(path, rb_intern("to_s"), 0);
  char *path_cstr = StringValueCStr(path_s);
  void *lib = dlopen(path_cstr, RTLD_NOW);
  const char *err = dlerror();
  if (err != NULL) {
    rb_raise(rb_eRuntimeError,
             "Could not load shared library `%s'.\nReason: %s", path_cstr, err);
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "tree_sitter_%s", StringValueCStr(name));
  tree_sitter_lang *make_ts_language = dlsym(lib, buf);
  err = dlerror();
  if (err != NULL) {
    dlclose(lib);
    rb_raise(rb_eRuntimeError,
             "Could not load symbol `%s' from library `%s'.\nReason:%s",
             StringValueCStr(name), StringValueCStr(path), err);
  }

  TSLanguage *lang = make_ts_language();
  if (lang == NULL) {
    dlclose(lib);
    rb_raise(rb_eRuntimeError,
             "TSLanguage = NULL for language `%s' in library `%s'.\nCall your "
             "local TSLanguage supplier.",
             StringValueCStr(name), StringValueCStr(path));
  }

  uint32_t version = ts_language_version(lang);
  if (version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) {
    rb_raise(rb_eRuntimeError,
             "Language %s (v%d) from `%s' is old.\nMinimum supported ABI: "
             "v%d.\nCurrent ABI: v%d.",
             StringValueCStr(name), version, StringValueCStr(path),
             TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION,
             TREE_SITTER_LANGUAGE_VERSION);
  }

  return new_language(lang);
}

static VALUE language_equal(VALUE self, VALUE other) {
  TSLanguage *this = SELF;
  TSLanguage *that = unwrap(other)->data;
  return this == that ? Qtrue : Qfalse;
}

void init_language(void) {
  cLanguage = rb_define_class_under(mTreeSitter, "Language", rb_cObject);

  rb_define_alloc_func(cLanguage, language_allocate);

  /* Class methods */
  rb_define_method(cLanguage, "symbol_count", language_symbol_count, 0);
  rb_define_method(cLanguage, "symbol_name", language_symbol_name, 1);
  rb_define_method(cLanguage, "symbol_for_name", language_symbol_for_name, 2);
  rb_define_method(cLanguage, "field_count", language_field_count, 0);
  rb_define_method(cLanguage, "field_name_for_id", language_field_name_for_id,
                   1);
  rb_define_method(cLanguage, "field_id_for_name", language_field_id_for_name,
                   1);
  rb_define_method(cLanguage, "symbol_type", language_symbol_type, 1);
  rb_define_method(cLanguage, "version", language_version, 0);
  rb_define_module_function(cLanguage, "load", language_load, 2);
  rb_define_method(cLanguage, "==", language_equal, 1);
}
