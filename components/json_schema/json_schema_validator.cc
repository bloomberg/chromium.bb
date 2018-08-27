// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/json_schema/json_schema_validator.h"

#include <stddef.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/json_schema/json_schema_constants.h"
#include "third_party/re2/src/re2/re2.h"

namespace schema = json_schema_constants;

namespace {

bool IsValidType(const std::string& type) {
  static const char* kValidTypes[] = {
    schema::kAny,
    schema::kArray,
    schema::kBoolean,
    schema::kInteger,
    schema::kNull,
    schema::kNumber,
    schema::kObject,
    schema::kString,
  };
  const char** end = kValidTypes + arraysize(kValidTypes);
  return std::find(kValidTypes, end, type) != end;
}

// Maps a schema attribute name to its expected type.
struct ExpectedType {
  const char* key;
  base::Value::Type type;
};

// Helper for std::lower_bound.
bool CompareToString(const ExpectedType& entry, const std::string& key) {
  return entry.key < key;
}

// If |value| is a dictionary, returns the "name" attribute of |value| or NULL
// if |value| does not contain a "name" attribute. Otherwise, returns |value|.
const base::Value* ExtractNameFromDictionary(const base::Value* value) {
  const base::DictionaryValue* value_dict = nullptr;
  const base::Value* name_value = nullptr;
  if (value->GetAsDictionary(&value_dict)) {
    value_dict->Get("name", &name_value);
    return name_value;
  }
  return value;
}

bool IsValidSchema(const base::DictionaryValue* dict,
                   int options,
                   std::string* error) {
  // This array must be sorted, so that std::lower_bound can perform a
  // binary search.
  static const ExpectedType kExpectedTypes[] = {
    // Note: kRef == "$ref", kSchema == "$schema"
    { schema::kRef,                     base::Value::Type::STRING      },
    { schema::kSchema,                  base::Value::Type::STRING      },

    { schema::kAdditionalProperties,    base::Value::Type::DICTIONARY  },
    { schema::kChoices,                 base::Value::Type::LIST        },
    { schema::kDescription,             base::Value::Type::STRING      },
    { schema::kEnum,                    base::Value::Type::LIST        },
    { schema::kId,                      base::Value::Type::STRING      },
    { schema::kMaxItems,                base::Value::Type::INTEGER     },
    { schema::kMaxLength,               base::Value::Type::INTEGER     },
    { schema::kMaximum,                 base::Value::Type::DOUBLE      },
    { schema::kMinItems,                base::Value::Type::INTEGER     },
    { schema::kMinLength,               base::Value::Type::INTEGER     },
    { schema::kMinimum,                 base::Value::Type::DOUBLE      },
    { schema::kOptional,                base::Value::Type::BOOLEAN     },
    { schema::kPattern,                 base::Value::Type::STRING      },
    { schema::kPatternProperties,       base::Value::Type::DICTIONARY  },
    { schema::kProperties,              base::Value::Type::DICTIONARY  },
    { schema::kRequired,                base::Value::Type::LIST        },
    { schema::kTitle,                   base::Value::Type::STRING      },
  };

  bool has_type_or_ref = false;
  const base::ListValue* list_value = nullptr;
  const base::DictionaryValue* dictionary_value = nullptr;
  std::string string_value;

  const base::ListValue* required_properties_value = nullptr;
  const base::DictionaryValue* properties_value = nullptr;

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    // Validate the "type" attribute, which may be a string or a list.
    if (it.key() == schema::kType) {
      switch (it.value().type()) {
        case base::Value::Type::STRING:
          it.value().GetAsString(&string_value);
          if (!IsValidType(string_value)) {
            *error = "Invalid value for type attribute";
            return false;
          }
          break;
        case base::Value::Type::LIST:
          it.value().GetAsList(&list_value);
          for (size_t i = 0; i < list_value->GetSize(); ++i) {
            if (!list_value->GetString(i, &string_value) ||
                !IsValidType(string_value)) {
              *error = "Invalid value for type attribute";
              return false;
            }
          }
          break;
        default:
          *error = "Invalid value for type attribute";
          return false;
      }
      has_type_or_ref = true;
      continue;
    }

    // Validate the "items" attribute, which is a schema or a list of schemas.
    if (it.key() == schema::kItems) {
      if (it.value().GetAsDictionary(&dictionary_value)) {
        if (!IsValidSchema(dictionary_value, options, error)) {
          DCHECK(!error->empty());
          return false;
        }
      } else if (it.value().GetAsList(&list_value)) {
        for (size_t i = 0; i < list_value->GetSize(); ++i) {
          if (!list_value->GetDictionary(i, &dictionary_value)) {
            *error = base::StringPrintf(
                "Invalid entry in items attribute at index %d",
                static_cast<int>(i));
            return false;
          }
          if (!IsValidSchema(dictionary_value, options, error)) {
            DCHECK(!error->empty());
            return false;
          }
        }
      } else {
        *error = "Invalid value for items attribute";
        return false;
      }
      continue;
    }

    // All the other attributes have a single valid type.
    const ExpectedType* end = kExpectedTypes + arraysize(kExpectedTypes);
    const ExpectedType* entry = std::lower_bound(
        kExpectedTypes, end, it.key(), CompareToString);
    if (entry == end || entry->key != it.key()) {
      if (options & JSONSchemaValidator::OPTIONS_IGNORE_UNKNOWN_ATTRIBUTES)
        continue;
      *error = base::StringPrintf("Invalid attribute %s", it.key().c_str());
      return false;
    }

    // Integer can be converted to double.
    if (!(it.value().type() == entry->type ||
          (it.value().is_int() && entry->type == base::Value::Type::DOUBLE))) {
      *error = base::StringPrintf("Invalid value for %s attribute",
                                  it.key().c_str());
      return false;
    }

    // base::Value::Type::INTEGER attributes must be >= 0.
    // This applies to "minItems", "maxItems", "minLength" and "maxLength".
    if (it.value().is_int()) {
      int integer_value;
      it.value().GetAsInteger(&integer_value);
      if (integer_value < 0) {
        *error = base::StringPrintf("Value of %s must be >= 0, got %d",
                                    it.key().c_str(), integer_value);
        return false;
      }
    }

    // Validate the "properties" attribute. Each entry maps a key to a schema.
    if (it.key() == schema::kProperties) {
      it.value().GetAsDictionary(&properties_value);
      for (base::DictionaryValue::Iterator iter(*properties_value);
           !iter.IsAtEnd(); iter.Advance()) {
        if (!iter.value().GetAsDictionary(&dictionary_value)) {
          *error = "properties must be a dictionary";
          return false;
        }
        if (!IsValidSchema(dictionary_value, options, error)) {
          DCHECK(!error->empty());
          return false;
        }
      }
    }

    // Validate the "patternProperties" attribute. Each entry maps a regular
    // expression to a schema. The validity of the regular expression expression
    // won't be checked here for performance reasons. Instead, invalid regular
    // expressions will be caught as validation errors in Validate().
    if (it.key() == schema::kPatternProperties) {
      it.value().GetAsDictionary(&dictionary_value);
      for (base::DictionaryValue::Iterator iter(*dictionary_value);
           !iter.IsAtEnd(); iter.Advance()) {
        if (!iter.value().GetAsDictionary(&dictionary_value)) {
          *error = "patternProperties must be a dictionary";
          return false;
        }
        if (!IsValidSchema(dictionary_value, options, error)) {
          DCHECK(!error->empty());
          return false;
        }
      }
    }

    // Validate "additionalProperties" attribute, which is a schema.
    if (it.key() == schema::kAdditionalProperties) {
      it.value().GetAsDictionary(&dictionary_value);
      if (!IsValidSchema(dictionary_value, options, error)) {
        DCHECK(!error->empty());
        return false;
      }
    }

    // Validate "required" attribute.
    if (it.key() == schema::kRequired) {
      it.value().GetAsList(&required_properties_value);
      for (const base::Value& value : *required_properties_value) {
        if (value.type() != base::Value::Type::STRING) {
          *error = "Invalid value in 'required' attribute";
          return false;
        }
      }
    }

    // Validate the values contained in an "enum" attribute.
    if (it.key() == schema::kEnum) {
      it.value().GetAsList(&list_value);
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        const base::Value* value = nullptr;
        list_value->Get(i, &value);
        // Sometimes the enum declaration is a dictionary with the enum value
        // under "name".
        value = ExtractNameFromDictionary(value);
        if (!value) {
          *error = "Invalid value in enum attribute";
          return false;
        }
        switch (value->type()) {
          case base::Value::Type::NONE:
          case base::Value::Type::BOOLEAN:
          case base::Value::Type::INTEGER:
          case base::Value::Type::DOUBLE:
          case base::Value::Type::STRING:
            break;
          default:
            *error = "Invalid value in enum attribute";
            return false;
        }
      }
    }

    // Validate the schemas contained in a "choices" attribute.
    if (it.key() == schema::kChoices) {
      it.value().GetAsList(&list_value);
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        if (!list_value->GetDictionary(i, &dictionary_value)) {
          *error = "Invalid choices attribute";
          return false;
        }
        if (!IsValidSchema(dictionary_value, options, error)) {
          DCHECK(!error->empty());
          return false;
        }
      }
    }

    if (it.key() == schema::kRef)
      has_type_or_ref = true;
  }

  // Check that properties in'required' are in the 'properties' object.
  if (required_properties_value) {
    for (const base::Value& value : required_properties_value->GetList()) {
      const std::string& name = value.GetString();
      if (!properties_value || !properties_value->HasKey(name)) {
        *error = "Property '" + name +
                 "' was listed in 'required', but not defined in 'properties'.";
        return false;
      }
    }
  }

  if (!has_type_or_ref) {
    *error = "Schema must have a type or a $ref attribute";
    return false;
  }

  return true;
}

}  // namespace

// static
std::unique_ptr<base::DictionaryValue> JSONSchemaValidator::IsValidSchema(
    const std::string& schema,
    std::string* error) {
  return JSONSchemaValidator::IsValidSchema(schema, 0, error);
}

// static
std::unique_ptr<base::DictionaryValue> JSONSchemaValidator::IsValidSchema(
    const std::string& schema,
    int validator_options,
    std::string* error) {
  base::JSONParserOptions json_options = base::JSON_PARSE_RFC;
  std::unique_ptr<base::Value> json = base::JSONReader::ReadAndReturnError(
      schema, json_options, nullptr, error);
  if (!json)
    return std::unique_ptr<base::DictionaryValue>();
  base::DictionaryValue* dict = nullptr;
  if (!json->GetAsDictionary(&dict)) {
    *error = "Schema must be a JSON object";
    return std::unique_ptr<base::DictionaryValue>();
  }
  if (!::IsValidSchema(dict, validator_options, error))
    return std::unique_ptr<base::DictionaryValue>();
  ignore_result(json.release());
  return base::WrapUnique(dict);
}
