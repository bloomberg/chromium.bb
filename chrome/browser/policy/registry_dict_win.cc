// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/registry_dict_win.h"

#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/common/json_schema/json_schema_constants.h"

namespace schema = json_schema_constants;

using base::win::RegistryKeyIterator;
using base::win::RegistryValueIterator;

namespace policy {

namespace {

// Returns the entry with key |name| in |dictionary| (can be NULL), or NULL.
const base::DictionaryValue* GetEntry(const base::DictionaryValue* dictionary,
                                      const std::string& name) {
  if (!dictionary)
    return NULL;
  const base::DictionaryValue* entry = NULL;
  dictionary->GetDictionaryWithoutPathExpansion(name, &entry);
  return entry;
}

// Returns the Value type described in |schema|, or |default_type| if not found.
base::Value::Type GetValueTypeForSchema(const base::DictionaryValue* schema,
                                        base::Value::Type default_type) {
  // JSON-schema types to base::Value::Type mapping.
  static const struct {
    // JSON schema type.
    const char* schema_type;
    // Correspondent value type.
    base::Value::Type value_type;
  } kSchemaToValueTypeMap[] = {
    { schema::kArray,        base::Value::TYPE_LIST        },
    { schema::kBoolean,      base::Value::TYPE_BOOLEAN     },
    { schema::kInteger,      base::Value::TYPE_INTEGER     },
    { schema::kNull,         base::Value::TYPE_NULL        },
    { schema::kNumber,       base::Value::TYPE_DOUBLE      },
    { schema::kObject,       base::Value::TYPE_DICTIONARY  },
    { schema::kString,       base::Value::TYPE_STRING      },
  };

  if (!schema)
    return default_type;
  std::string type;
  if (!schema->GetStringWithoutPathExpansion(schema::kType, &type))
    return default_type;
  for (size_t i = 0; i < arraysize(kSchemaToValueTypeMap); ++i) {
    if (type == kSchemaToValueTypeMap[i].schema_type)
      return kSchemaToValueTypeMap[i].value_type;
  }
  return default_type;
}

// Returns the schema for property |name| given the |schema| of an object.
// Returns the "additionalProperties" schema if no specific schema for
// |name| is present. Returns NULL if no schema is found.
const base::DictionaryValue* GetSchemaFor(const base::DictionaryValue* schema,
                                          const std::string& name) {
  const base::DictionaryValue* properties =
      GetEntry(schema, schema::kProperties);
  const base::DictionaryValue* sub_schema = GetEntry(properties, name);
  if (sub_schema)
    return sub_schema;
  // "additionalProperties" can be a boolean, but that case is ignored.
  return GetEntry(schema, schema::kAdditionalProperties);
}

// Converts a value (as read from the registry) to meet |schema|, converting
// types as necessary. Unconvertible types will show up as NULL values in the
// result.
scoped_ptr<base::Value> ConvertValue(const base::Value& value,
                                     const base::DictionaryValue* schema) {
  // Figure out the type to convert to from the schema.
  const base::Value::Type result_type(
      GetValueTypeForSchema(schema, value.GetType()));

  // If the type is good already, go with it.
  if (value.IsType(result_type)) {
    // Recurse for complex types if there is a schema.
    if (schema) {
      const base::DictionaryValue* dict = NULL;
      const base::ListValue* list = NULL;
      if (value.GetAsDictionary(&dict)) {
        scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
        for (base::DictionaryValue::Iterator entry(*dict); !entry.IsAtEnd();
             entry.Advance()) {
          scoped_ptr<base::Value> converted_value(
              ConvertValue(entry.value(), GetSchemaFor(schema, entry.key())));
          result->SetWithoutPathExpansion(entry.key(),
                                          converted_value.release());
        }
        return result.Pass();
      } else if (value.GetAsList(&list)) {
        scoped_ptr<base::ListValue> result(new base::ListValue());
        const base::DictionaryValue* item_schema =
            GetEntry(schema, schema::kItems);
        for (base::ListValue::const_iterator entry(list->begin());
             entry != list->end(); ++entry) {
          result->Append(ConvertValue(**entry, item_schema).release());
        }
        return result.Pass();
      }
    }
    return make_scoped_ptr(value.DeepCopy());
  }

  // Else, do some conversions to map windows registry data types to JSON types.
  std::string string_value;
  int int_value = 0;
  switch (result_type) {
    case base::Value::TYPE_NULL: {
      return make_scoped_ptr(base::Value::CreateNullValue());
    }
    case base::Value::TYPE_BOOLEAN: {
      // Accept booleans encoded as either string or integer.
      if (value.GetAsInteger(&int_value) ||
          (value.GetAsString(&string_value) &&
           base::StringToInt(string_value, &int_value))) {
        return make_scoped_ptr(Value::CreateBooleanValue(int_value != 0));
      }
      break;
    }
    case base::Value::TYPE_INTEGER: {
      // Integers may be string-encoded.
      if (value.GetAsString(&string_value) &&
          base::StringToInt(string_value, &int_value)) {
        return make_scoped_ptr(base::Value::CreateIntegerValue(int_value));
      }
      break;
    }
    case base::Value::TYPE_DOUBLE: {
      // Doubles may be string-encoded or integer-encoded.
      double double_value = 0;
      if (value.GetAsInteger(&int_value)) {
        return make_scoped_ptr(base::Value::CreateDoubleValue(int_value));
      } else if (value.GetAsString(&string_value) &&
                 base::StringToDouble(string_value, &double_value)) {
        return make_scoped_ptr(base::Value::CreateDoubleValue(double_value));
      }
      break;
    }
    case base::Value::TYPE_LIST: {
      // Lists are encoded as subkeys with numbered value in the registry.
      const base::DictionaryValue* dict = NULL;
      if (value.GetAsDictionary(&dict)) {
        scoped_ptr<base::ListValue> result(new base::ListValue());
        const base::DictionaryValue* item_schema =
            GetEntry(schema, schema::kItems);
        for (int i = 1; ; ++i) {
          const base::Value* entry = NULL;
          if (!dict->Get(base::IntToString(i), &entry))
            break;
          result->Append(ConvertValue(*entry, item_schema).release());
        }
        return result.Pass();
      }
      // Fall through in order to accept lists encoded as JSON strings.
    }
    case base::Value::TYPE_DICTIONARY: {
      // Dictionaries may be encoded as JSON strings.
      if (value.GetAsString(&string_value)) {
        scoped_ptr<base::Value> result(base::JSONReader::Read(string_value));
        if (result && result->IsType(result_type))
          return result.Pass();
      }
      break;
    }
    case base::Value::TYPE_STRING:
    case base::Value::TYPE_BINARY:
      // No conversion possible.
      break;
  }

  LOG(WARNING) << "Failed to convert " << value.GetType()
               << " to " << result_type;
  return make_scoped_ptr(base::Value::CreateNullValue());
}

}  // namespace

bool CaseInsensitiveStringCompare::operator()(const std::string& a,
                                              const std::string& b) const {
  return base::strcasecmp(a.c_str(), b.c_str()) < 0;
}

RegistryDict::RegistryDict() {}

RegistryDict::~RegistryDict() {
  ClearKeys();
  ClearValues();
}

RegistryDict* RegistryDict::GetKey(const std::string& name) {
  KeyMap::iterator entry = keys_.find(name);
  return entry != keys_.end() ? entry->second : NULL;
}

const RegistryDict* RegistryDict::GetKey(const std::string& name) const {
  KeyMap::const_iterator entry = keys_.find(name);
  return entry != keys_.end() ? entry->second : NULL;
}

void RegistryDict::SetKey(const std::string& name,
                          scoped_ptr<RegistryDict> dict) {
  if (!dict) {
    RemoveKey(name);
    return;
  }

  RegistryDict*& entry = keys_[name];
  delete entry;
  entry = dict.release();
}

scoped_ptr<RegistryDict> RegistryDict::RemoveKey(const std::string& name) {
  scoped_ptr<RegistryDict> result;
  KeyMap::iterator entry = keys_.find(name);
  if (entry != keys_.end()) {
    result.reset(entry->second);
    keys_.erase(entry);
  }
  return result.Pass();
}

void RegistryDict::ClearKeys() {
  STLDeleteValues(&keys_);
}

base::Value* RegistryDict::GetValue(const std::string& name) {
  ValueMap::iterator entry = values_.find(name);
  return entry != values_.end() ? entry->second : NULL;
}

const base::Value* RegistryDict::GetValue(const std::string& name) const {
  ValueMap::const_iterator entry = values_.find(name);
  return entry != values_.end() ? entry->second : NULL;
}

void RegistryDict::SetValue(const std::string& name,
                            scoped_ptr<base::Value> dict) {
  if (!dict) {
    RemoveValue(name);
    return;
  }

  Value*& entry = values_[name];
  delete entry;
  entry = dict.release();
}

scoped_ptr<base::Value> RegistryDict::RemoveValue(const std::string& name) {
  scoped_ptr<base::Value> result;
  ValueMap::iterator entry = values_.find(name);
  if (entry != values_.end()) {
    result.reset(entry->second);
    values_.erase(entry);
  }
  return result.Pass();
}

void RegistryDict::ClearValues() {
  STLDeleteValues(&values_);
}

void RegistryDict::Merge(const RegistryDict& other) {
  for (KeyMap::const_iterator entry(other.keys_.begin());
       entry != other.keys_.end(); ++entry) {
    RegistryDict*& subdict = keys_[entry->first];
    if (!subdict)
      subdict = new RegistryDict();
    subdict->Merge(*entry->second);
  }

  for (ValueMap::const_iterator entry(other.values_.begin());
       entry != other.values_.end(); ++entry) {
    SetValue(entry->first, make_scoped_ptr(entry->second->DeepCopy()));
  }
}

void RegistryDict::Swap(RegistryDict* other) {
  keys_.swap(other->keys_);
  values_.swap(other->values_);
}

void RegistryDict::ReadRegistry(HKEY hive, const string16& root) {
  ClearKeys();
  ClearValues();

  // First, read all the values of the key.
  for (RegistryValueIterator it(hive, root.c_str()); it.Valid(); ++it) {
    const std::string name = UTF16ToUTF8(it.Name());
    switch (it.Type()) {
      case REG_SZ:
      case REG_EXPAND_SZ:
        SetValue(
            name,
            make_scoped_ptr(new base::StringValue(UTF16ToUTF8(it.Value()))));
        continue;
      case REG_DWORD_LITTLE_ENDIAN:
      case REG_DWORD_BIG_ENDIAN:
        if (it.ValueSize() == sizeof(DWORD)) {
          DWORD dword_value = *(reinterpret_cast<const DWORD*>(it.Value()));
          if (it.Type() == REG_DWORD_BIG_ENDIAN)
            dword_value = base::NetToHost32(dword_value);
          else
            dword_value = base::ByteSwapToLE32(dword_value);
          SetValue(
              name,
              make_scoped_ptr(base::Value::CreateIntegerValue(dword_value)));
          continue;
        }
      case REG_NONE:
      case REG_LINK:
      case REG_MULTI_SZ:
      case REG_RESOURCE_LIST:
      case REG_FULL_RESOURCE_DESCRIPTOR:
      case REG_RESOURCE_REQUIREMENTS_LIST:
      case REG_QWORD_LITTLE_ENDIAN:
        // Unsupported type, message gets logged below.
        break;
    }

    LOG(WARNING) << "Failed to read hive " << hive << " at "
                 << root << "\\" << name
                 << " type " << it.Type();
  }

  // Recurse for all subkeys.
  for (RegistryKeyIterator it(hive, root.c_str()); it.Valid(); ++it) {
    std::string name(UTF16ToUTF8(it.Name()));
    scoped_ptr<RegistryDict> subdict(new RegistryDict());
    subdict->ReadRegistry(hive, root + L"\\" + it.Name());
    SetKey(name, subdict.Pass());
  }
}

scoped_ptr<base::Value> RegistryDict::ConvertToJSON(
    const base::DictionaryValue* schema) const {
  base::Value::Type type =
      GetValueTypeForSchema(schema, base::Value::TYPE_DICTIONARY);
  switch (type) {
    case base::Value::TYPE_DICTIONARY: {
      scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
      for (RegistryDict::ValueMap::const_iterator entry(values_.begin());
           entry != values_.end(); ++entry) {
        result->SetWithoutPathExpansion(
            entry->first,
            ConvertValue(*entry->second,
                         GetSchemaFor(schema, entry->first)).release());
      }
      for (RegistryDict::KeyMap::const_iterator entry(keys_.begin());
           entry != keys_.end(); ++entry) {
        result->SetWithoutPathExpansion(
            entry->first,
            entry->second->ConvertToJSON(
                GetSchemaFor(schema, entry->first)).release());
      }
      return result.Pass();
    }
    case base::Value::TYPE_LIST: {
      scoped_ptr<base::ListValue> result(new base::ListValue());
      const base::DictionaryValue* item_schema =
          GetEntry(schema, schema::kItems);
      for (int i = 1; ; ++i) {
        const std::string name(base::IntToString(i));
        const RegistryDict* key = GetKey(name);
        if (key) {
          result->Append(key->ConvertToJSON(item_schema).release());
          continue;
        }
        const base::Value* value = GetValue(name);
        if (value) {
          result->Append(ConvertValue(*value, item_schema).release());
          continue;
        }
        break;
      }
      return result.Pass();
    }
    default:
      LOG(WARNING) << "Can't convert registry key to schema type " << type;
  }

  return make_scoped_ptr(base::Value::CreateNullValue());
}

}  // namespace policy
