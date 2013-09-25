// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "components/json_schema/json_schema_constants.h"
#include "components/json_schema/json_schema_validator.h"
#include "components/policy/core/common/schema_internal.h"

namespace policy {

namespace {

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

}  // namespace

Schema::Iterator::Iterator(const internal::PropertiesNode* properties)
    : it_(properties->begin),
      end_(properties->end) {}

Schema::Iterator::Iterator(const Iterator& iterator)
    : it_(iterator.it_),
      end_(iterator.end_) {}

Schema::Iterator::~Iterator() {}

Schema::Iterator& Schema::Iterator::operator=(const Iterator& iterator) {
  it_ = iterator.it_;
  end_ = iterator.end_;
  return *this;
}

bool Schema::Iterator::IsAtEnd() const {
  return it_ == end_;
}

void Schema::Iterator::Advance() {
  ++it_;
}

const char* Schema::Iterator::key() const {
  return it_->key;
}

Schema Schema::Iterator::schema() const {
  return Schema(it_->schema);
}

Schema::Schema() : schema_(NULL) {}

Schema::Schema(const internal::SchemaNode* schema) : schema_(schema) {}

Schema::Schema(const Schema& schema) : schema_(schema.schema_) {}

Schema& Schema::operator=(const Schema& schema) {
  schema_ = schema.schema_;
  return *this;
}

base::Value::Type Schema::type() const {
  CHECK(valid());
  return schema_->type;
}

Schema::Iterator Schema::GetPropertiesIterator() const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_DICTIONARY, type());
  return Iterator(
      static_cast<const internal::PropertiesNode*>(schema_->extra));
}

namespace {

bool CompareKeys(const internal::PropertyNode& node, const std::string& key) {
  return node.key < key;
}

}  // namespace

Schema Schema::GetKnownProperty(const std::string& key) const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_DICTIONARY, type());
  const internal::PropertiesNode* properties_node =
      static_cast<const internal::PropertiesNode*>(schema_->extra);
  const internal::PropertyNode* it = std::lower_bound(
      properties_node->begin, properties_node->end, key, CompareKeys);
  if (it != properties_node->end && it->key == key)
    return Schema(it->schema);
  return Schema();
}

Schema Schema::GetAdditionalProperties() const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_DICTIONARY, type());
  return Schema(
      static_cast<const internal::PropertiesNode*>(schema_->extra)->additional);
}

Schema Schema::GetProperty(const std::string& key) const {
  Schema schema = GetKnownProperty(key);
  return schema.valid() ? schema : GetAdditionalProperties();
}

Schema Schema::GetItems() const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_LIST, type());
  return Schema(static_cast<const internal::SchemaNode*>(schema_->extra));
}

SchemaOwner::SchemaOwner(const internal::SchemaNode* root) : root_(root) {}

SchemaOwner::~SchemaOwner() {
  for (size_t i = 0; i < property_nodes_.size(); ++i)
    delete[] property_nodes_[i];
}

// static
scoped_ptr<SchemaOwner> SchemaOwner::Wrap(const internal::SchemaNode* schema) {
  return scoped_ptr<SchemaOwner>(new SchemaOwner(schema));
}

// static
scoped_ptr<SchemaOwner> SchemaOwner::Parse(const std::string& content,
                                           std::string* error) {
  // Validate as a generic JSON schema.
  scoped_ptr<base::DictionaryValue> dict =
      JSONSchemaValidator::IsValidSchema(content, error);
  if (!dict)
    return scoped_ptr<SchemaOwner>();

  // Validate the main type.
  std::string string_value;
  if (!dict->GetString(json_schema_constants::kType, &string_value) ||
      string_value != json_schema_constants::kObject) {
    *error =
        "The main schema must have a type attribute with \"object\" value.";
    return scoped_ptr<SchemaOwner>();
  }

  // Checks for invalid attributes at the top-level.
  if (dict->HasKey(json_schema_constants::kAdditionalProperties) ||
      dict->HasKey(json_schema_constants::kPatternProperties)) {
    *error = "\"additionalProperties\" and \"patternProperties\" are not "
             "supported at the main schema.";
    return scoped_ptr<SchemaOwner>();
  }

  scoped_ptr<SchemaOwner> impl(new SchemaOwner(NULL));
  impl->root_ = impl->Parse(*dict, error);
  if (!impl->root_)
    impl.reset();
  return impl.PassAs<SchemaOwner>();
}

const internal::SchemaNode* SchemaOwner::Parse(
    const base::DictionaryValue& schema,
    std::string* error) {
  std::string type_string;
  if (!schema.GetString(json_schema_constants::kType, &type_string)) {
    *error = "The schema type must be declared.";
    return NULL;
  }

  base::Value::Type type = base::Value::TYPE_NULL;
  if (!SchemaTypeToValueType(type_string, &type)) {
    *error = "Type not supported: " + type_string;
    return NULL;
  }

  if (type == base::Value::TYPE_DICTIONARY)
    return ParseDictionary(schema, error);
  if (type == base::Value::TYPE_LIST)
    return ParseList(schema, error);

  internal::SchemaNode* node = new internal::SchemaNode;
  node->type = type;
  node->extra = NULL;
  schema_nodes_.push_back(node);
  return node;
}

const internal::SchemaNode* SchemaOwner::ParseDictionary(
    const base::DictionaryValue& schema,
    std::string* error) {
  internal::PropertiesNode* properties_node = new internal::PropertiesNode;
  properties_node->begin = NULL;
  properties_node->end = NULL;
  properties_node->additional = NULL;
  properties_nodes_.push_back(properties_node);

  const base::DictionaryValue* dict = NULL;
  const base::DictionaryValue* properties = NULL;
  if (schema.GetDictionary(json_schema_constants::kProperties, &properties)) {
    internal::PropertyNode* property_nodes =
        new internal::PropertyNode[properties->size()];
    property_nodes_.push_back(property_nodes);

    size_t index = 0;
    for (base::DictionaryValue::Iterator it(*properties);
         !it.IsAtEnd(); it.Advance(), ++index) {
      // This should have been verified by the JSONSchemaValidator.
      CHECK(it.value().GetAsDictionary(&dict));
      const internal::SchemaNode* sub_schema = Parse(*dict, error);
      if (!sub_schema)
        return NULL;
      std::string* key = new std::string(it.key());
      keys_.push_back(key);
      property_nodes[index].key = key->c_str();
      property_nodes[index].schema = sub_schema;
    }
    CHECK_EQ(properties->size(), index);
    properties_node->begin = property_nodes;
    properties_node->end = property_nodes + index;
  }

  if (schema.GetDictionary(json_schema_constants::kAdditionalProperties,
                           &dict)) {
    const internal::SchemaNode* sub_schema = Parse(*dict, error);
    if (!sub_schema)
      return NULL;
    properties_node->additional = sub_schema;
  }

  internal::SchemaNode* schema_node = new internal::SchemaNode;
  schema_node->type = base::Value::TYPE_DICTIONARY;
  schema_node->extra = properties_node;
  schema_nodes_.push_back(schema_node);
  return schema_node;
}

const internal::SchemaNode* SchemaOwner::ParseList(
    const base::DictionaryValue& schema,
    std::string* error) {
  const base::DictionaryValue* dict = NULL;
  if (!schema.GetDictionary(json_schema_constants::kItems, &dict)) {
    *error = "Arrays must declare a single schema for their items.";
    return NULL;
  }
  const internal::SchemaNode* items_schema_node = Parse(*dict, error);
  if (!items_schema_node)
    return NULL;

  internal::SchemaNode* schema_node = new internal::SchemaNode;
  schema_node->type = base::Value::TYPE_LIST;
  schema_node->extra = items_schema_node;
  schema_nodes_.push_back(schema_node);
  return schema_node;
}

}  // namespace policy
