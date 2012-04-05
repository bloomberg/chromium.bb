// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/gvalue_util.h"

#include <dbus/dbus-glib.h>

#include "base/logging.h"
#include "base/values.h"

namespace chromeos {

namespace {

// Converts a bool to a GValue.
GValue* ConvertBoolToGValue(bool b) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_BOOLEAN);
  g_value_set_boolean(gvalue, b);
  return gvalue;
}

// Converts an int to a GValue.
GValue* ConvertIntToGValue(int i) {
  // Converting to a 32-bit signed int type in particular, since
  // that's what flimflam expects in its DBus API
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_INT);
  g_value_set_int(gvalue, i);
  return gvalue;
}

// Converts a string to a GValue.
GValue* ConvertStringToGValue(const std::string& s) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_STRING);
  g_value_set_string(gvalue, s.c_str());
  return gvalue;
}

// Converts a DictionaryValue to a GValue containing a string-to-string
// GHashTable.
GValue* ConvertDictionaryValueToGValue(const DictionaryValue& dict) {
  GHashTable* ghash =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  for (DictionaryValue::key_iterator it = dict.begin_keys();
       it != dict.end_keys(); ++it) {
    std::string key = *it;
    std::string val;
    if (!dict.GetString(key, &val)) {
      NOTREACHED() << "Invalid type in dictionary, key: " << key;
      continue;
    }
    g_hash_table_insert(ghash,
                        g_strdup(const_cast<char*>(key.c_str())),
                        g_strdup(const_cast<char*>(val.c_str())));
  }
  GValue* gvalue = new GValue();
  g_value_init(gvalue, DBUS_TYPE_G_STRING_STRING_HASHTABLE);
  g_value_take_boxed(gvalue, ghash);
  return gvalue;
}

// Unsets and deletes the GValue.
void UnsetAndDeleteGValue(gpointer ptr) {
  GValue* gvalue = static_cast<GValue*>(ptr);
  g_value_unset(gvalue);
  delete gvalue;
}

void AppendListElement(const GValue* gvalue, gpointer user_data) {
  ListValue* list = static_cast<ListValue*>(user_data);
  Value* value = ConvertGValueToValue(gvalue);
  list->Append(value);
}

void AppendDictionaryElement(const GValue* keyvalue,
                             const GValue* gvalue,
                             gpointer user_data) {
  DictionaryValue* dict = static_cast<DictionaryValue*>(user_data);
  std::string key(g_value_get_string(keyvalue));
  Value* value = ConvertGValueToValue(gvalue);
  dict->SetWithoutPathExpansion(key, value);
}

}  // namespace

ScopedGValue::ScopedGValue() {}

ScopedGValue::ScopedGValue(GValue* value) : value_(value) {}

ScopedGValue::~ScopedGValue() {
  reset(NULL);
}

void ScopedGValue::reset(GValue* value) {
  if (value_.get())
    g_value_unset(value_.get());
  value_.reset(value);
}

ScopedGHashTable::ScopedGHashTable() : table_(NULL) {}

ScopedGHashTable::ScopedGHashTable(GHashTable* table) : table_(table) {}

ScopedGHashTable::~ScopedGHashTable() {
  reset(NULL);
}

void ScopedGHashTable::reset(GHashTable* table) {
  if (table_)
    g_hash_table_unref(table_);
  table_ = table;
}

GHashTable* ConvertDictionaryValueToStringValueGHashTable(
    const DictionaryValue& dict) {
  GHashTable* ghash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            UnsetAndDeleteGValue);
  for (DictionaryValue::key_iterator it = dict.begin_keys();
       it != dict.end_keys(); ++it) {
    std::string key = *it;
    Value* val = NULL;
    if (dict.GetWithoutPathExpansion(key, &val)) {
      g_hash_table_insert(ghash,
                          g_strdup(const_cast<char*>(key.c_str())),
                          ConvertValueToGValue(*val));
    } else {
      VLOG(2) << "Could not insert key " << key << " into hash";
    }
  }
  return ghash;
}

GValue* ConvertValueToGValue(const Value& value) {
  switch (value.GetType()) {
    case Value::TYPE_BOOLEAN: {
      bool out;
      if (value.GetAsBoolean(&out))
        return ConvertBoolToGValue(out);
      break;
    }
    case Value::TYPE_INTEGER: {
      int out;
      if (value.GetAsInteger(&out))
        return ConvertIntToGValue(out);
      break;
    }
    case Value::TYPE_STRING: {
      std::string out;
      if (value.GetAsString(&out))
        return ConvertStringToGValue(out);
      break;
    }
    case Value::TYPE_DICTIONARY: {
      const DictionaryValue& dict = static_cast<const DictionaryValue&>(value);
      return ConvertDictionaryValueToGValue(dict);
    }
    case Value::TYPE_NULL:
    case Value::TYPE_DOUBLE:
    case Value::TYPE_BINARY:
    case Value::TYPE_LIST:
      // Other Value types shouldn't be passed through this mechanism.
      NOTREACHED() << "Unconverted Value of type: " << value.GetType();
      return new GValue();
  }
  NOTREACHED() << "Value conversion failed, type: " << value.GetType();
  return new GValue();
}

Value* ConvertGValueToValue(const GValue* gvalue) {
  if (G_VALUE_HOLDS_STRING(gvalue)) {
    return Value::CreateStringValue(g_value_get_string(gvalue));
  } else if (G_VALUE_HOLDS_BOOLEAN(gvalue)) {
    return Value::CreateBooleanValue(
        static_cast<bool>(g_value_get_boolean(gvalue)));
  } else if (G_VALUE_HOLDS_INT(gvalue)) {
    return Value::CreateIntegerValue(g_value_get_int(gvalue));
  } else if (G_VALUE_HOLDS_UINT(gvalue)) {
    return Value::CreateIntegerValue(
        static_cast<int>(g_value_get_uint(gvalue)));
  } else if (G_VALUE_HOLDS_UCHAR(gvalue)) {
    return Value::CreateIntegerValue(
        static_cast<int>(g_value_get_uchar(gvalue)));
  } else if (G_VALUE_HOLDS(gvalue, DBUS_TYPE_G_OBJECT_PATH)) {
    const char* path = static_cast<const char*>(g_value_get_boxed(gvalue));
    return Value::CreateStringValue(path);
  } else if (G_VALUE_HOLDS(gvalue, G_TYPE_STRV)) {
    ListValue* list = new ListValue();
    for (GStrv strv = static_cast<GStrv>(g_value_get_boxed(gvalue));
         *strv != NULL; ++strv) {
      list->Append(Value::CreateStringValue(*strv));
    }
    return list;
  } else if (dbus_g_type_is_collection(G_VALUE_TYPE(gvalue))) {
    ListValue* list = new ListValue();
    dbus_g_type_collection_value_iterate(gvalue, AppendListElement, list);
    return list;
  } else if (dbus_g_type_is_map(G_VALUE_TYPE(gvalue))) {
    DictionaryValue* dict = new DictionaryValue();
    dbus_g_type_map_value_iterate(gvalue, AppendDictionaryElement, dict);
    return dict;
  } else if (G_VALUE_HOLDS(gvalue, G_TYPE_VALUE)) {
    const GValue* bvalue = static_cast<GValue*>(g_value_get_boxed(gvalue));
    return ConvertGValueToValue(bvalue);
  } else {
    LOG(ERROR) << "Unrecognized Glib value type: " << G_VALUE_TYPE(gvalue);
    return Value::CreateNullValue();
  }
}

DictionaryValue* ConvertStringValueGHashTableToDictionaryValue(
    GHashTable* ghash) {
  DictionaryValue* dict = new DictionaryValue();
  GHashTableIter iter;
  gpointer gkey, gvalue;
  g_hash_table_iter_init(&iter, ghash);
  while (g_hash_table_iter_next(&iter, &gkey, &gvalue))  {
    std::string key(static_cast<char*>(gkey));
    Value* value = ConvertGValueToValue(static_cast<GValue*>(gvalue));
    dict->SetWithoutPathExpansion(key, value);
  }
  return dict;
}

}  // namespace chromeos
