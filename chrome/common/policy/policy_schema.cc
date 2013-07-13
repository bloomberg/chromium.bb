// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/policy/policy_schema.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/common/json_schema/json_schema_constants.h"
#include "chrome/common/json_schema/json_schema_validator.h"

namespace policy {

namespace {

const char kJSONSchemaVersion[] = "http://json-schema.org/draft-03/schema#";

// Describes the properties of a TYPE_DICTIONARY policy schema.
class DictionaryPolicySchema : public PolicySchema {
 public:
  static scoped_ptr<PolicySchema> Parse(const base::DictionaryValue& schema,
                                        std::string* error);

  virtual ~DictionaryPolicySchema();

  virtual const PolicySchemaMap* GetProperties() const OVERRIDE;
  virtual const PolicySchema* GetSchemaForAdditionalProperties() const OVERRIDE;

 private:
  DictionaryPolicySchema();

  PolicySchemaMap properties_;
  scoped_ptr<PolicySchema> additional_properties_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryPolicySchema);
};

// Describes the items of a TYPE_LIST policy schema.
class ListPolicySchema : public PolicySchema {
 public:
  static scoped_ptr<PolicySchema> Parse(const base::DictionaryValue& schema,
                                        std::string* error);

  virtual ~ListPolicySchema();

  virtual const PolicySchema* GetSchemaForItems() const OVERRIDE;

 private:
  ListPolicySchema();

  scoped_ptr<PolicySchema> items_schema_;

  DISALLOW_COPY_AND_ASSIGN(ListPolicySchema);
};

bool SchemaTypeToValueType(const std::string& type_string,
                           base::Value::Type* type) {
  // Note: "any" is not an accepted type.
  static const struct {
    const char* schema_type;
    base::Value::Type value_type;
  } kSchemaToValueTypeMap[] = {
    { json_schema_constants::kArray,        base::Value::TYPE_LIST       },
    { json_schema_constants::kBoolean,      base::Value::TYPE_BOOLEAN    },
    { json_schema_constants::kInteger,      base::Value::TYPE_INTEGER    },
    { json_schema_constants::kNull,         base::Value::TYPE_NULL       },
    { json_schema_constants::kNumber,       base::Value::TYPE_DOUBLE     },
    { json_schema_constants::kObject,       base::Value::TYPE_DICTIONARY },
    { json_schema_constants::kString,       base::Value::TYPE_STRING     },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSchemaToValueTypeMap); ++i) {
    if (kSchemaToValueTypeMap[i].schema_type == type_string) {
      *type = kSchemaToValueTypeMap[i].value_type;
      return true;
    }
  }
  return false;
}

scoped_ptr<PolicySchema> ParseSchema(const base::DictionaryValue& schema,
                                     std::string* error) {
  std::string type_string;
  if (!schema.GetString(json_schema_constants::kType, &type_string)) {
    *error = "The schema type must be declared.";
    return scoped_ptr<PolicySchema>();
  }

  base::Value::Type type = base::Value::TYPE_NULL;
  if (!SchemaTypeToValueType(type_string, &type)) {
    *error = "The \"any\" type can't be used.";
    return scoped_ptr<PolicySchema>();
  }

  switch (type) {
    case base::Value::TYPE_DICTIONARY:
      return DictionaryPolicySchema::Parse(schema, error);
    case base::Value::TYPE_LIST:
      return ListPolicySchema::Parse(schema, error);
    default:
      return make_scoped_ptr(new PolicySchema(type));
  }
}

DictionaryPolicySchema::DictionaryPolicySchema()
    : PolicySchema(base::Value::TYPE_DICTIONARY) {}

DictionaryPolicySchema::~DictionaryPolicySchema() {
  STLDeleteValues(&properties_);
}

const PolicySchemaMap* DictionaryPolicySchema::GetProperties() const {
  return &properties_;
}

const PolicySchema*
    DictionaryPolicySchema::GetSchemaForAdditionalProperties() const {
  return additional_properties_.get();
}

// static
scoped_ptr<PolicySchema> DictionaryPolicySchema::Parse(
    const base::DictionaryValue& schema,
    std::string* error) {
  scoped_ptr<DictionaryPolicySchema> dict_schema(new DictionaryPolicySchema());

  const base::DictionaryValue* dict = NULL;
  const base::DictionaryValue* properties = NULL;
  if (schema.GetDictionary(json_schema_constants::kProperties, &properties)) {
    for (base::DictionaryValue::Iterator it(*properties);
         !it.IsAtEnd(); it.Advance()) {
      // This should have been verified by the JSONSchemaValidator.
      CHECK(it.value().GetAsDictionary(&dict));
      scoped_ptr<PolicySchema> sub_schema = ParseSchema(*dict, error);
      if (!sub_schema)
        return scoped_ptr<PolicySchema>();
      dict_schema->properties_[it.key()] = sub_schema.release();
    }
  }

  if (schema.GetDictionary(json_schema_constants::kAdditionalProperties,
                           &dict)) {
    scoped_ptr<PolicySchema> sub_schema = ParseSchema(*dict, error);
    if (!sub_schema)
      return scoped_ptr<PolicySchema>();
    dict_schema->additional_properties_ = sub_schema.Pass();
  }

  return dict_schema.PassAs<PolicySchema>();
}

ListPolicySchema::ListPolicySchema()
    : PolicySchema(base::Value::TYPE_LIST) {}

ListPolicySchema::~ListPolicySchema() {}

const PolicySchema* ListPolicySchema::GetSchemaForItems() const {
  return items_schema_.get();
}

scoped_ptr<PolicySchema> ListPolicySchema::Parse(
    const base::DictionaryValue& schema,
    std::string* error) {
  const base::DictionaryValue* dict = NULL;
  if (!schema.GetDictionary(json_schema_constants::kItems, &dict)) {
    *error = "Arrays must declare a single schema for their items.";
    return scoped_ptr<PolicySchema>();
  }
  scoped_ptr<PolicySchema> items_schema = ParseSchema(*dict, error);
  if (!items_schema)
    return scoped_ptr<PolicySchema>();

  scoped_ptr<ListPolicySchema> list_schema(new ListPolicySchema());
  list_schema->items_schema_ = items_schema.Pass();
  return list_schema.PassAs<PolicySchema>();
}

}  // namespace

PolicySchema::PolicySchema(base::Value::Type type)
    : type_(type) {}

PolicySchema::~PolicySchema() {}

const PolicySchemaMap* PolicySchema::GetProperties() const {
  NOTREACHED();
  return NULL;
}

const PolicySchema* PolicySchema::GetSchemaForAdditionalProperties() const {
  NOTREACHED();
  return NULL;
}

const PolicySchema* PolicySchema::GetSchemaForProperty(
    const std::string& key) const {
  const PolicySchemaMap* properties = GetProperties();
  PolicySchemaMap::const_iterator it = properties->find(key);
  return it == properties->end() ? GetSchemaForAdditionalProperties()
                                 : it->second;
}

const PolicySchema* PolicySchema::GetSchemaForItems() const {
  NOTREACHED();
  return NULL;
}

// static
scoped_ptr<PolicySchema> PolicySchema::Parse(const std::string& content,
                                             std::string* error) {
  // Validate as a generic JSON schema.
  scoped_ptr<base::DictionaryValue> dict =
      JSONSchemaValidator::IsValidSchema(content, error);
  if (!dict)
    return scoped_ptr<PolicySchema>();

  // Validate the schema version.
  std::string string_value;
  if (!dict->GetString(json_schema_constants::kSchema, &string_value) ||
      string_value != kJSONSchemaVersion) {
    *error = "Must declare JSON Schema v3 version in \"$schema\".";
    return scoped_ptr<PolicySchema>();
  }

  // Validate the main type.
  if (!dict->GetString(json_schema_constants::kType, &string_value) ||
      string_value != json_schema_constants::kObject) {
    *error =
        "The main schema must have a type attribute with \"object\" value.";
    return scoped_ptr<PolicySchema>();
  }

  // Checks for invalid attributes at the top-level.
  if (dict->HasKey(json_schema_constants::kAdditionalProperties) ||
      dict->HasKey(json_schema_constants::kPatternProperties)) {
    *error = "\"additionalProperties\" and \"patternProperties\" are not "
             "supported at the main schema.";
    return scoped_ptr<PolicySchema>();
  }

  return ParseSchema(*dict, error);
}

}  // namespace policy
