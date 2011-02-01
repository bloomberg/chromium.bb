// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/json_schema_validator.h"

#include <cfloat>
#include <cmath>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

double GetNumberValue(Value* value) {
  double result = 0;
  if (value->GetAsDouble(&result))
    return result;

  int int_result = 0;
  if (value->GetAsInteger(&int_result)) {
    return int_result;
  }

  CHECK(false) << "Unexpected value type: " << value->GetType();
  return 0;
}

bool GetNumberFromDictionary(DictionaryValue* value, const std::string& key,
                             double* number) {
  if (value->GetDouble(key, number))
    return true;

  int int_value = 0;
  if (value->GetInteger(key, &int_value)) {
    *number = int_value;
    return true;
  }

  return false;
}

}  // namespace


JSONSchemaValidator::Error::Error() {
}

JSONSchemaValidator::Error::Error(const std::string& message)
    : path(message) {
}

JSONSchemaValidator::Error::Error(const std::string& path,
                                  const std::string& message)
    : path(path), message(message) {
}


const char JSONSchemaValidator::kUnknownTypeReference[] =
    "Unknown schema reference: *.";
const char JSONSchemaValidator::kInvalidChoice[] =
    "Value does not match any valid type choices.";
const char JSONSchemaValidator::kInvalidEnum[] =
    "Value does not match any valid enum choices.";
const char JSONSchemaValidator::kObjectPropertyIsRequired[] =
    "Property is required.";
const char JSONSchemaValidator::kUnexpectedProperty[] =
    "Unexpected property.";
const char JSONSchemaValidator::kArrayMinItems[] =
    "Array must have at least * items.";
const char JSONSchemaValidator::kArrayMaxItems[] =
    "Array must not have more than * items.";
const char JSONSchemaValidator::kArrayItemRequired[] =
    "Item is required.";
const char JSONSchemaValidator::kStringMinLength[] =
    "String must be at least * characters long.";
const char JSONSchemaValidator::kStringMaxLength[] =
    "String must not be more than * characters long.";
const char JSONSchemaValidator::kStringPattern[] =
    "String must match the pattern: *.";
const char JSONSchemaValidator::kNumberMinimum[] =
    "Value must not be less than *.";
const char JSONSchemaValidator::kNumberMaximum[] =
    "Value must not be greater than *.";
const char JSONSchemaValidator::kInvalidType[] =
    "Expected '*' but got '*'.";


// static
std::string JSONSchemaValidator::GetJSONSchemaType(Value* value) {
  switch (value->GetType()) {
    case Value::TYPE_NULL:
      return "null";
    case Value::TYPE_BOOLEAN:
      return "boolean";
    case Value::TYPE_INTEGER:
      return "integer";
    case Value::TYPE_DOUBLE: {
      double double_value = 0;
      value->GetAsDouble(&double_value);
      if (std::abs(double_value) <= std::pow(2.0, DBL_MANT_DIG) &&
          double_value == floor(double_value)) {
        return "integer";
      } else {
        return "number";
      }
    }
    case Value::TYPE_STRING:
      return "string";
    case Value::TYPE_DICTIONARY:
      return "object";
    case Value::TYPE_LIST:
      return "array";
    default:
      CHECK(false) << "Unexpected value type: " << value->GetType();
      return "";
  }
}

// static
std::string JSONSchemaValidator::FormatErrorMessage(const std::string& format,
                                                    const std::string& s1) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  return ret_val;
}

// static
std::string JSONSchemaValidator::FormatErrorMessage(const std::string& format,
                                                    const std::string& s1,
                                                    const std::string& s2) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  return ret_val;
}

JSONSchemaValidator::JSONSchemaValidator(DictionaryValue* schema)
    : schema_root_(schema), default_allow_additional_properties_(false) {
}

JSONSchemaValidator::JSONSchemaValidator(DictionaryValue* schema,
                                         ListValue* types)
    : schema_root_(schema), default_allow_additional_properties_(false) {
  if (!types)
    return;

  for (size_t i = 0; i < types->GetSize(); ++i) {
    DictionaryValue* type = NULL;
    CHECK(types->GetDictionary(i, &type));

    std::string id;
    CHECK(type->GetString("id", &id));

    CHECK(types_.find(id) == types_.end());
    types_[id] = type;
  }
}

JSONSchemaValidator::~JSONSchemaValidator() {}

bool JSONSchemaValidator::Validate(Value* instance) {
  errors_.clear();
  Validate(instance, schema_root_, "");
  return errors_.empty();
}

void JSONSchemaValidator::Validate(Value* instance,
                                   DictionaryValue* schema,
                                   const std::string& path) {
  // If this schema defines itself as reference type, save it in this.types.
  std::string id;
  if (schema->GetString("id", &id)) {
    TypeMap::iterator iter = types_.find(id);
    if (iter == types_.end())
      types_[id] = schema;
    else
      CHECK(iter->second == schema);
  }

  // If the schema has a $ref property, the instance must validate against
  // that schema. It must be present in types_ to be referenced.
  std::string ref;
  if (schema->GetString("$ref", &ref)) {
    TypeMap::iterator type = types_.find(ref);
    if (type == types_.end()) {
      errors_.push_back(
          Error(path, FormatErrorMessage(kUnknownTypeReference, ref)));
    } else {
      Validate(instance, type->second, path);
    }
    return;
  }

  // If the schema has a choices property, the instance must validate against at
  // least one of the items in that array.
  ListValue* choices = NULL;
  if (schema->GetList("choices", &choices)) {
    ValidateChoices(instance, choices, path);
    return;
  }

  // If the schema has an enum property, the instance must be one of those
  // values.
  ListValue* enumeration = NULL;
  if (schema->GetList("enum", &enumeration)) {
    ValidateEnum(instance, enumeration, path);
    return;
  }

  std::string type;
  schema->GetString("type", &type);
  CHECK(!type.empty());
  if (type != "any") {
    if (!ValidateType(instance, type, path))
      return;

    // These casts are safe because of checks in ValidateType().
    if (type == "object")
      ValidateObject(static_cast<DictionaryValue*>(instance), schema, path);
    else if (type == "array")
      ValidateArray(static_cast<ListValue*>(instance), schema, path);
    else if (type == "string")
      ValidateString(static_cast<StringValue*>(instance), schema, path);
    else if (type == "number" || type == "integer")
      ValidateNumber(instance, schema, path);
    else if (type != "boolean" && type != "null")
      CHECK(false) << "Unexpected type: " << type;
  }
}

void JSONSchemaValidator::ValidateChoices(Value* instance,
                                          ListValue* choices,
                                          const std::string& path) {
  size_t original_num_errors = errors_.size();

  for (size_t i = 0; i < choices->GetSize(); ++i) {
    DictionaryValue* choice = NULL;
    CHECK(choices->GetDictionary(i, &choice));

    Validate(instance, choice, path);
    if (errors_.size() == original_num_errors)
      return;

    // We discard the error from each choice. We only want to know if any of the
    // validations succeeded.
    errors_.resize(original_num_errors);
  }

  // Now add a generic error that no choices matched.
  errors_.push_back(Error(path, kInvalidChoice));
  return;
}

void JSONSchemaValidator::ValidateEnum(Value* instance,
                                       ListValue* choices,
                                       const std::string& path) {
  for (size_t i = 0; i < choices->GetSize(); ++i) {
    Value* choice = NULL;
    CHECK(choices->Get(i, &choice));
    switch (choice->GetType()) {
      case Value::TYPE_NULL:
      case Value::TYPE_BOOLEAN:
      case Value::TYPE_STRING:
        if (instance->Equals(choice))
          return;
        break;

      case Value::TYPE_INTEGER:
      case Value::TYPE_DOUBLE:
        if (instance->IsType(Value::TYPE_INTEGER) ||
            instance->IsType(Value::TYPE_DOUBLE)) {
          if (GetNumberValue(choice) == GetNumberValue(instance))
            return;
        }
        break;

      default:
        CHECK(false) << "Unexpected type in enum: " << choice->GetType();
    }
  }

  errors_.push_back(Error(path, kInvalidEnum));
}

void JSONSchemaValidator::ValidateObject(DictionaryValue* instance,
                                         DictionaryValue* schema,
                                         const std::string& path) {
  DictionaryValue* properties = NULL;
  schema->GetDictionary("properties", &properties);
  if (properties) {
    for (DictionaryValue::key_iterator key = properties->begin_keys();
         key != properties->end_keys(); ++key) {
      std::string prop_path = path.empty() ? *key : (path + "." + *key);
      DictionaryValue* prop_schema = NULL;
      CHECK(properties->GetDictionary(*key, &prop_schema));

      Value* prop_value = NULL;
      if (instance->Get(*key, &prop_value)) {
        Validate(prop_value, prop_schema, prop_path);
      } else {
        // Properties are required unless there is an optional field set to
        // 'true'.
        bool is_optional = false;
        prop_schema->GetBoolean("optional", &is_optional);
        if (!is_optional) {
          errors_.push_back(Error(prop_path, kObjectPropertyIsRequired));
        }
      }
    }
  }

  DictionaryValue* additional_properties_schema = NULL;
  if (SchemaAllowsAnyAdditionalItems(schema, &additional_properties_schema))
    return;

  // Validate additional properties.
  for (DictionaryValue::key_iterator key = instance->begin_keys();
       key != instance->end_keys(); ++key) {
    if (properties && properties->HasKey(*key))
      continue;

    std::string prop_path = path.empty() ? *key : path + "." + *key;
    if (!additional_properties_schema) {
      errors_.push_back(Error(prop_path, kUnexpectedProperty));
    } else {
      Value* prop_value = NULL;
      CHECK(instance->Get(*key, &prop_value));
      Validate(prop_value, additional_properties_schema, prop_path);
    }
  }
}

void JSONSchemaValidator::ValidateArray(ListValue* instance,
                                        DictionaryValue* schema,
                                        const std::string& path) {
  DictionaryValue* single_type = NULL;
  size_t instance_size = instance->GetSize();
  if (schema->GetDictionary("items", &single_type)) {
    int min_items = 0;
    if (schema->GetInteger("minItems", &min_items)) {
      CHECK(min_items >= 0);
      if (instance_size < static_cast<size_t>(min_items)) {
        errors_.push_back(Error(path, FormatErrorMessage(
            kArrayMinItems, base::IntToString(min_items))));
      }
    }

    int max_items = 0;
    if (schema->GetInteger("maxItems", &max_items)) {
      CHECK(max_items >= 0);
      if (instance_size > static_cast<size_t>(max_items)) {
        errors_.push_back(Error(path, FormatErrorMessage(
            kArrayMaxItems, base::IntToString(max_items))));
      }
    }

    // If the items property is a single schema, each item in the array must
    // validate against that schema.
    for (size_t i = 0; i < instance_size; ++i) {
      Value* item = NULL;
      CHECK(instance->Get(i, &item));
      std::string i_str = base::UintToString(i);
      std::string item_path = path.empty() ? i_str : (path + "." + i_str);
      Validate(item, single_type, item_path);
    }

    return;
  }

  // Otherwise, the list must be a tuple type, where each item in the list has a
  // particular schema.
  ValidateTuple(instance, schema, path);
}

void JSONSchemaValidator::ValidateTuple(ListValue* instance,
                                        DictionaryValue* schema,
                                        const std::string& path) {
  ListValue* tuple_type = NULL;
  schema->GetList("items", &tuple_type);
  size_t tuple_size = tuple_type ? tuple_type->GetSize() : 0;
  if (tuple_type) {
    for (size_t i = 0; i < tuple_size; ++i) {
      std::string i_str = base::UintToString(i);
      std::string item_path = path.empty() ? i_str : (path + "." + i_str);
      DictionaryValue* item_schema = NULL;
      CHECK(tuple_type->GetDictionary(i, &item_schema));
      Value* item_value = NULL;
      instance->Get(i, &item_value);
      if (item_value && item_value->GetType() != Value::TYPE_NULL) {
        Validate(item_value, item_schema, item_path);
      } else {
        bool is_optional = false;
        item_schema->GetBoolean("optional", &is_optional);
        if (!is_optional) {
          errors_.push_back(Error(item_path, kArrayItemRequired));
          return;
        }
      }
    }
  }

  DictionaryValue* additional_properties_schema = NULL;
  if (SchemaAllowsAnyAdditionalItems(schema, &additional_properties_schema))
    return;

  size_t instance_size = instance->GetSize();
  if (additional_properties_schema) {
    // Any additional properties must validate against the additionalProperties
    // schema.
    for (size_t i = tuple_size; i < instance_size; ++i) {
      std::string i_str = base::UintToString(i);
      std::string item_path = path.empty() ? i_str : (path + "." + i_str);
      Value* item_value = NULL;
      CHECK(instance->Get(i, &item_value));
      Validate(item_value, additional_properties_schema, item_path);
    }
  } else if (instance_size > tuple_size) {
    errors_.push_back(Error(path, FormatErrorMessage(
        kArrayMaxItems, base::UintToString(tuple_size))));
  }
}

void JSONSchemaValidator::ValidateString(StringValue* instance,
                                         DictionaryValue* schema,
                                         const std::string& path) {
  std::string value;
  CHECK(instance->GetAsString(&value));

  int min_length = 0;
  if (schema->GetInteger("minLength", &min_length)) {
    CHECK(min_length >= 0);
    if (value.size() < static_cast<size_t>(min_length)) {
      errors_.push_back(Error(path, FormatErrorMessage(
          kStringMinLength, base::IntToString(min_length))));
    }
  }

  int max_length = 0;
  if (schema->GetInteger("maxLength", &max_length)) {
    CHECK(max_length >= 0);
    if (value.size() > static_cast<size_t>(max_length)) {
      errors_.push_back(Error(path, FormatErrorMessage(
          kStringMaxLength, base::IntToString(max_length))));
    }
  }

  CHECK(!schema->HasKey("pattern")) << "Pattern is not supported.";
}

void JSONSchemaValidator::ValidateNumber(Value* instance,
                                         DictionaryValue* schema,
                                         const std::string& path) {
  double value = GetNumberValue(instance);

  // TODO(aa): It would be good to test that the double is not infinity or nan,
  // but isnan and isinf aren't defined on Windows.

  double minimum = 0;
  if (GetNumberFromDictionary(schema, "minimum", &minimum)) {
    if (value < minimum)
      errors_.push_back(Error(path, FormatErrorMessage(
          kNumberMinimum, base::DoubleToString(minimum))));
  }

  double maximum = 0;
  if (GetNumberFromDictionary(schema, "maximum", &maximum)) {
    if (value > maximum)
      errors_.push_back(Error(path, FormatErrorMessage(
          kNumberMaximum, base::DoubleToString(maximum))));
  }
}

bool JSONSchemaValidator::ValidateType(Value* instance,
                                       const std::string& expected_type,
                                       const std::string& path) {
  std::string actual_type = GetJSONSchemaType(instance);
  if (expected_type == actual_type ||
      (expected_type == "number" && actual_type == "integer")) {
    return true;
  } else {
    errors_.push_back(Error(path, FormatErrorMessage(
        kInvalidType, expected_type, actual_type)));
    return false;
  }
}

bool JSONSchemaValidator::SchemaAllowsAnyAdditionalItems(
    DictionaryValue* schema, DictionaryValue** additional_properties_schema) {
  // If the validator allows additional properties globally, and this schema
  // doesn't override, then we can exit early.
  schema->GetDictionary("additionalProperties", additional_properties_schema);

  if (*additional_properties_schema) {
    std::string additional_properties_type("any");
    CHECK((*additional_properties_schema)->GetString(
        "type", &additional_properties_type));
    return additional_properties_type == "any";
  } else {
    return default_allow_additional_properties_;
  }
}
