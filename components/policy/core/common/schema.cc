// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "components/json_schema/json_schema_constants.h"
#include "components/json_schema/json_schema_validator.h"
#include "components/policy/core/common/schema_internal.h"

namespace schema = json_schema_constants;

namespace policy {

using internal::PropertiesNode;
using internal::PropertyNode;
using internal::SchemaData;
using internal::SchemaNode;

namespace {

// Maps schema "id" attributes to the corresponding SchemaNode index.
typedef std::map<std::string, int> IdMap;

// List of pairs of references to be assigned later. The string is the "id"
// whose corresponding index should be stored in the pointer, once all the IDs
// are available.
typedef std::vector<std::pair<std::string, int*> > ReferenceList;

// Sizes for the storage arrays. These are calculated in advance so that the
// arrays don't have to be resized during parsing, which would invalidate
// pointers into their contents (i.e. string's c_str() and address of indices
// for "$ref" attributes).
struct StorageSizes {
  StorageSizes()
      : strings(0), schema_nodes(0), property_nodes(0), properties_nodes(0) {}
  size_t strings;
  size_t schema_nodes;
  size_t property_nodes;
  size_t properties_nodes;
};

// An invalid index, indicating that a node is not present; similar to a NULL
// pointer.
const int kInvalid = -1;

bool SchemaTypeToValueType(const std::string& type_string,
                           base::Value::Type* type) {
  // Note: "any" is not an accepted type.
  static const struct {
    const char* schema_type;
    base::Value::Type value_type;
  } kSchemaToValueTypeMap[] = {
    { schema::kArray,        base::Value::TYPE_LIST       },
    { schema::kBoolean,      base::Value::TYPE_BOOLEAN    },
    { schema::kInteger,      base::Value::TYPE_INTEGER    },
    { schema::kNull,         base::Value::TYPE_NULL       },
    { schema::kNumber,       base::Value::TYPE_DOUBLE     },
    { schema::kObject,       base::Value::TYPE_DICTIONARY },
    { schema::kString,       base::Value::TYPE_STRING     },
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

// Contains the internal data representation of a Schema. This can either wrap
// a SchemaData owned elsewhere (currently used to wrap the Chrome schema, which
// is generated at compile time), or it can own its own SchemaData.
class Schema::InternalStorage
    : public base::RefCountedThreadSafe<InternalStorage> {
 public:
  static scoped_refptr<const InternalStorage> Wrap(const SchemaData* data);

  static scoped_refptr<const InternalStorage> ParseSchema(
      const base::DictionaryValue& schema,
      std::string* error);

  const SchemaData* data() const { return &schema_data_; }

  const SchemaNode* root_node() const {
    return schema(0);
  }

  const SchemaNode* schema(int index) const {
    return schema_data_.schema_nodes + index;
  }

  const PropertiesNode* properties(int index) const {
    return schema_data_.properties_nodes + index;
  }

  const PropertyNode* property(int index) const {
    return schema_data_.property_nodes + index;
  }

 private:
  friend class base::RefCountedThreadSafe<InternalStorage>;

  InternalStorage();
  ~InternalStorage();

  // Determines the expected |sizes| of the storage for the representation
  // of |schema|.
  static void DetermineStorageSizes(const base::DictionaryValue& schema,
                                   StorageSizes* sizes);

  // Parses the JSON schema in |schema|.
  //
  // If |schema| has a "$ref" attribute then a pending reference is appended
  // to the |reference_list|, and nothing else is done.
  //
  // Otherwise, |index| gets assigned the index of the corresponding SchemaNode
  // in |schema_nodes_|. If the |schema| contains an "id" then that ID is mapped
  // to the |index| in the |id_map|.
  //
  // If |schema| is invalid then |error| gets the error reason and false is
  // returned. Otherwise returns true.
  bool Parse(const base::DictionaryValue& schema,
             int* index,
             IdMap* id_map,
             ReferenceList* reference_list,
             std::string* error);

  // Helper for Parse() that gets an already assigned |schema_node| instead of
  // an |index| pointer.
  bool ParseDictionary(const base::DictionaryValue& schema,
                       SchemaNode* schema_node,
                       IdMap* id_map,
                       ReferenceList* reference_list,
                       std::string* error);

  // Helper for Parse() that gets an already assigned |schema_node| instead of
  // an |index| pointer.
  bool ParseList(const base::DictionaryValue& schema,
                 SchemaNode* schema_node,
                 IdMap* id_map,
                 ReferenceList* reference_list,
                 std::string* error);

  // Assigns the IDs in |id_map| to the pending references in the
  // |reference_list|. If an ID is missing then |error| is set and false is
  // returned; otherwise returns true.
  static bool ResolveReferences(const IdMap& id_map,
                                const ReferenceList& reference_list,
                                std::string* error);

  SchemaData schema_data_;
  std::vector<std::string> strings_;
  std::vector<SchemaNode> schema_nodes_;
  std::vector<PropertyNode> property_nodes_;
  std::vector<PropertiesNode> properties_nodes_;

  DISALLOW_COPY_AND_ASSIGN(InternalStorage);
};

Schema::InternalStorage::InternalStorage() {}

Schema::InternalStorage::~InternalStorage() {}

// static
scoped_refptr<const Schema::InternalStorage> Schema::InternalStorage::Wrap(
    const SchemaData* data) {
  InternalStorage* storage = new InternalStorage();
  storage->schema_data_.schema_nodes = data->schema_nodes;
  storage->schema_data_.property_nodes = data->property_nodes;
  storage->schema_data_.properties_nodes = data->properties_nodes;
  return storage;
}

// static
scoped_refptr<const Schema::InternalStorage>
Schema::InternalStorage::ParseSchema(const base::DictionaryValue& schema,
                                     std::string* error) {
  // Determine the sizes of the storage arrays and reserve the capacity before
  // starting to append nodes and strings. This is important to prevent the
  // arrays from being reallocated, which would invalidate the c_str() pointers
  // and the addresses of indices to fix.
  StorageSizes sizes;
  DetermineStorageSizes(schema, &sizes);

  scoped_refptr<InternalStorage> storage = new InternalStorage();
  storage->strings_.reserve(sizes.strings);
  storage->schema_nodes_.reserve(sizes.schema_nodes);
  storage->property_nodes_.reserve(sizes.property_nodes);
  storage->properties_nodes_.reserve(sizes.properties_nodes);

  int root_index = kInvalid;
  IdMap id_map;
  ReferenceList reference_list;
  if (!storage->Parse(schema, &root_index, &id_map, &reference_list, error))
    return NULL;

  if (root_index == kInvalid) {
    *error = "The main schema can't have a $ref";
    return NULL;
  }

  // None of this should ever happen without having been already detected.
  // But, if it does happen, then it will lead to corrupted memory; drop
  // everything in that case.
  if (root_index != 0 ||
      sizes.strings != storage->strings_.size() ||
      sizes.schema_nodes != storage->schema_nodes_.size() ||
      sizes.property_nodes != storage->property_nodes_.size() ||
      sizes.properties_nodes != storage->properties_nodes_.size()) {
    *error = "Failed to parse the schema due to a Chrome bug. Please file a "
             "new issue at http://crbug.com";
    return NULL;
  }

  if (!ResolveReferences(id_map, reference_list, error))
    return NULL;

  SchemaData* data = &storage->schema_data_;
  data->schema_nodes = vector_as_array(&storage->schema_nodes_);
  data->property_nodes = vector_as_array(&storage->property_nodes_);
  data->properties_nodes = vector_as_array(&storage->properties_nodes_);
  return storage;
}

// static
void Schema::InternalStorage::DetermineStorageSizes(
    const base::DictionaryValue& schema,
    StorageSizes* sizes) {
  std::string ref_string;
  if (schema.GetString(schema::kRef, &ref_string)) {
    // Schemas with a "$ref" attribute don't take additional storage.
    return;
  }

  std::string type_string;
  base::Value::Type type = base::Value::TYPE_NULL;
  if (!schema.GetString(schema::kType, &type_string) ||
      !SchemaTypeToValueType(type_string, &type)) {
    // This schema is invalid.
    return;
  }

  sizes->schema_nodes++;

  if (type == base::Value::TYPE_LIST) {
    const base::DictionaryValue* items = NULL;
    if (schema.GetDictionary(schema::kItems, &items))
      DetermineStorageSizes(*items, sizes);
  } else if (type == base::Value::TYPE_DICTIONARY) {
    sizes->properties_nodes++;

    const base::DictionaryValue* dict = NULL;
    if (schema.GetDictionary(schema::kAdditionalProperties, &dict))
      DetermineStorageSizes(*dict, sizes);

    const base::DictionaryValue* properties = NULL;
    if (schema.GetDictionary(schema::kProperties, &properties)) {
      for (base::DictionaryValue::Iterator it(*properties);
           !it.IsAtEnd(); it.Advance()) {
        // This should have been verified by the JSONSchemaValidator.
        CHECK(it.value().GetAsDictionary(&dict));
        DetermineStorageSizes(*dict, sizes);
        sizes->strings++;
        sizes->property_nodes++;
      }
    }
  }
}

bool Schema::InternalStorage::Parse(const base::DictionaryValue& schema,
                                    int* index,
                                    IdMap* id_map,
                                    ReferenceList* reference_list,
                                    std::string* error) {
  std::string ref_string;
  if (schema.GetString(schema::kRef, &ref_string)) {
    std::string id_string;
    if (schema.GetString(schema::kId, &id_string)) {
      *error = "Schemas with a $ref can't have an id";
      return false;
    }
    reference_list->push_back(std::make_pair(ref_string, index));
    return true;
  }

  std::string type_string;
  if (!schema.GetString(schema::kType, &type_string)) {
    *error = "The schema type must be declared.";
    return false;
  }

  base::Value::Type type = base::Value::TYPE_NULL;
  if (!SchemaTypeToValueType(type_string, &type)) {
    *error = "Type not supported: " + type_string;
    return false;
  }

  *index = static_cast<int>(schema_nodes_.size());
  schema_nodes_.push_back(SchemaNode());
  SchemaNode* schema_node = &schema_nodes_.back();
  schema_node->type = type;
  schema_node->extra = kInvalid;

  if (type == base::Value::TYPE_DICTIONARY) {
    if (!ParseDictionary(schema, schema_node, id_map, reference_list, error))
      return false;
  } else if (type == base::Value::TYPE_LIST) {
    if (!ParseList(schema, schema_node, id_map, reference_list, error))
      return false;
  }

  std::string id_string;
  if (schema.GetString(schema::kId, &id_string)) {
    if (ContainsKey(*id_map, id_string)) {
      *error = "Duplicated id: " + id_string;
      return false;
    }
    (*id_map)[id_string] = *index;
  }

  return true;
}

bool Schema::InternalStorage::ParseDictionary(
    const base::DictionaryValue& schema,
    SchemaNode* schema_node,
    IdMap* id_map,
    ReferenceList* reference_list,
    std::string* error) {
  int extra = static_cast<int>(properties_nodes_.size());
  properties_nodes_.push_back(PropertiesNode());
  properties_nodes_[extra].begin = kInvalid;
  properties_nodes_[extra].end = kInvalid;
  properties_nodes_[extra].additional = kInvalid;
  schema_node->extra = extra;

  const base::DictionaryValue* dict = NULL;
  if (schema.GetDictionary(schema::kAdditionalProperties, &dict)) {
    if (!Parse(*dict, &properties_nodes_[extra].additional,
               id_map, reference_list, error)) {
      return false;
    }
  }

  const base::DictionaryValue* properties = NULL;
  if (schema.GetDictionary(schema::kProperties, &properties)) {
    int base_index = static_cast<int>(property_nodes_.size());
    // This reserves nodes for all of the |properties|, and makes sure they
    // are contiguous. Recursive calls to Parse() will append after these
    // elements.
    property_nodes_.resize(base_index + properties->size());

    int index = base_index;
    for (base::DictionaryValue::Iterator it(*properties);
         !it.IsAtEnd(); it.Advance(), ++index) {
      // This should have been verified by the JSONSchemaValidator.
      CHECK(it.value().GetAsDictionary(&dict));
      strings_.push_back(it.key());
      property_nodes_[index].key = strings_.back().c_str();
      if (!Parse(*dict, &property_nodes_[index].schema,
                 id_map, reference_list, error)) {
        return false;
      }
    }
    CHECK_EQ(static_cast<int>(properties->size()), index - base_index);
    properties_nodes_[extra].begin = base_index;
    properties_nodes_[extra].end = index;
  }

  return true;
}

bool Schema::InternalStorage::ParseList(const base::DictionaryValue& schema,
                                        SchemaNode* schema_node,
                                        IdMap* id_map,
                                        ReferenceList* reference_list,
                                        std::string* error) {
  const base::DictionaryValue* dict = NULL;
  if (!schema.GetDictionary(schema::kItems, &dict)) {
    *error = "Arrays must declare a single schema for their items.";
    return false;
  }
  return Parse(*dict, &schema_node->extra, id_map, reference_list, error);
}

// static
bool Schema::InternalStorage::ResolveReferences(
    const IdMap& id_map,
    const ReferenceList& reference_list,
    std::string* error) {
  for (ReferenceList::const_iterator ref = reference_list.begin();
       ref != reference_list.end(); ++ref) {
    IdMap::const_iterator id = id_map.find(ref->first);
    if (id == id_map.end()) {
      *error = "Invalid $ref: " + ref->first;
      return false;
    }
    *ref->second = id->second;
  }
  return true;
}

Schema::Iterator::Iterator(const scoped_refptr<const InternalStorage>& storage,
                           const PropertiesNode* node)
    : storage_(storage),
      it_(storage->property(node->begin)),
      end_(storage->property(node->end)) {}

Schema::Iterator::Iterator(const Iterator& iterator)
    : storage_(iterator.storage_),
      it_(iterator.it_),
      end_(iterator.end_) {}

Schema::Iterator::~Iterator() {}

Schema::Iterator& Schema::Iterator::operator=(const Iterator& iterator) {
  storage_ = iterator.storage_;
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
  return Schema(storage_, storage_->schema(it_->schema));
}

Schema::Schema() : node_(NULL) {}

Schema::Schema(const scoped_refptr<const InternalStorage>& storage,
               const SchemaNode* node)
    : storage_(storage), node_(node) {}

Schema::Schema(const Schema& schema)
    : storage_(schema.storage_), node_(schema.node_) {}

Schema::~Schema() {}

Schema& Schema::operator=(const Schema& schema) {
  storage_ = schema.storage_;
  node_ = schema.node_;
  return *this;
}

// static
Schema Schema::Wrap(const SchemaData* data) {
  scoped_refptr<const InternalStorage> storage = InternalStorage::Wrap(data);
  return Schema(storage, storage->root_node());
}

bool Schema::Validate(const base::Value& value) const {
  if (!valid()) {
    // Schema not found, invalid entry.
    return false;
  }

  if (!value.IsType(type()))
    return false;

  const base::DictionaryValue* dict = NULL;
  const base::ListValue* list = NULL;
  if (value.GetAsDictionary(&dict)) {
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      if (!GetProperty(it.key()).Validate(it.value()))
        return false;
    }
  } else if (value.GetAsList(&list)) {
    for (base::ListValue::const_iterator it = list->begin();
         it != list->end(); ++it) {
      if (!*it || !GetItems().Validate(**it))
        return false;
    }
  }

  return true;
}

// static
Schema Schema::Parse(const std::string& content, std::string* error) {
  // Validate as a generic JSON schema, and ignore unknown attributes; they
  // may become used in a future version of the schema format.
  scoped_ptr<base::DictionaryValue> dict = JSONSchemaValidator::IsValidSchema(
      content, JSONSchemaValidator::OPTIONS_IGNORE_UNKNOWN_ATTRIBUTES, error);
  if (!dict)
    return Schema();

  // Validate the main type.
  std::string string_value;
  if (!dict->GetString(schema::kType, &string_value) ||
      string_value != schema::kObject) {
    *error =
        "The main schema must have a type attribute with \"object\" value.";
    return Schema();
  }

  // Checks for invalid attributes at the top-level.
  if (dict->HasKey(schema::kAdditionalProperties) ||
      dict->HasKey(schema::kPatternProperties)) {
    *error = "\"additionalProperties\" and \"patternProperties\" are not "
             "supported at the main schema.";
    return Schema();
  }

  scoped_refptr<const InternalStorage> storage =
      InternalStorage::ParseSchema(*dict, error);
  if (!storage)
    return Schema();
  return Schema(storage, storage->root_node());
}

base::Value::Type Schema::type() const {
  CHECK(valid());
  return node_->type;
}

Schema::Iterator Schema::GetPropertiesIterator() const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_DICTIONARY, type());
  return Iterator(storage_, storage_->properties(node_->extra));
}

namespace {

bool CompareKeys(const PropertyNode& node, const std::string& key) {
  return node.key < key;
}

}  // namespace

Schema Schema::GetKnownProperty(const std::string& key) const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_DICTIONARY, type());
  const PropertiesNode* node = storage_->properties(node_->extra);
  const PropertyNode* begin = storage_->property(node->begin);
  const PropertyNode* end = storage_->property(node->end);
  const PropertyNode* it = std::lower_bound(begin, end, key, CompareKeys);
  if (it != end && it->key == key)
    return Schema(storage_, storage_->schema(it->schema));
  return Schema();
}

Schema Schema::GetAdditionalProperties() const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_DICTIONARY, type());
  const PropertiesNode* node = storage_->properties(node_->extra);
  if (node->additional == kInvalid)
    return Schema();
  return Schema(storage_, storage_->schema(node->additional));
}

Schema Schema::GetProperty(const std::string& key) const {
  Schema schema = GetKnownProperty(key);
  return schema.valid() ? schema : GetAdditionalProperties();
}

Schema Schema::GetItems() const {
  CHECK(valid());
  CHECK_EQ(base::Value::TYPE_LIST, type());
  if (node_->extra == kInvalid)
    return Schema();
  return Schema(storage_, storage_->schema(node_->extra));
}

}  // namespace policy
