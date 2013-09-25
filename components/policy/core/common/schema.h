// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

struct POLICY_EXPORT SchemaNode;
struct POLICY_EXPORT PropertyNode;
struct POLICY_EXPORT PropertiesNode;

}  // namespace internal

// Describes the expected type of one policy. Also recursively describes the
// types of inner elements, for structured types.
// Objects of this class refer to external, immutable data and are cheap to
// copy.
// Use the SchemaOwner class to parse a schema and get Schema objects.
class POLICY_EXPORT Schema {
 public:
  // Builds an empty, invalid schema.
  Schema();

  // Builds a schema pointing to the inner structure of |schema|. If |schema|
  // is NULL then this Schema instance will be invalid.
  // Does not take ownership of |schema|.
  explicit Schema(const internal::SchemaNode* schema);

  Schema(const Schema& schema);

  Schema& operator=(const Schema& schema);

  // Returns true if this Schema is valid. Schemas returned by the methods below
  // may be invalid, and in those cases the other methods must not be used.
  bool valid() const { return schema_ != NULL; }

  base::Value::Type type() const;

  // Used to iterate over the known properties of TYPE_DICTIONARY schemas.
  class POLICY_EXPORT Iterator {
   public:
    explicit Iterator(const internal::PropertiesNode* properties);
    Iterator(const Iterator& iterator);
    ~Iterator();

    Iterator& operator=(const Iterator& iterator);

    // The other methods must not be called if the iterator is at the end.
    bool IsAtEnd() const;

    // Advances the iterator to the next property.
    void Advance();

    // Returns the name of the current property.
    const char* key() const;

    // Returns the Schema for the current property. This Schema is always valid.
    Schema schema() const;

   private:
    const internal::PropertyNode* it_;
    const internal::PropertyNode* end_;
  };

  // These methods should be called only if type() == TYPE_DICTIONARY,
  // otherwise invalid memory will be read. A CHECK is currently enforcing this.

  // Returns an iterator that goes over the named properties of this schema.
  // The returned iterator is at the beginning.
  Iterator GetPropertiesIterator() const;

  // Returns the Schema for the property named |key|. If |key| is not a known
  // property name then the returned Schema is not valid.
  Schema GetKnownProperty(const std::string& key) const;

  // Returns the Schema for additional properties. If additional properties are
  // not allowed for this Schema then the Schema returned is not valid.
  Schema GetAdditionalProperties() const;

  // Returns the Schema for |key| if it is a known property, otherwise returns
  // the Schema for additional properties.
  Schema GetProperty(const std::string& key) const;

  // Returns the Schema for items of an array.
  // This method should be called only if type() == TYPE_LIST,
  // otherwise invalid memory will be read. A CHECK is currently enforcing this.
  Schema GetItems() const;

 private:
  const internal::SchemaNode* schema_;
};

// Owns schemas for policies of a given component.
class POLICY_EXPORT SchemaOwner {
 public:
  ~SchemaOwner();

  // The returned Schema is valid only during the lifetime of the SchemaOwner
  // that created it. It may be obtained multiple times.
  Schema schema() const { return Schema(root_); }

  // Returns a SchemaOwner that references static data. This can be used by
  // the embedder to pass structures generated at compile time, which can then
  // be quickly loaded at runtime.
  // Note: PropertiesNodes must have their PropertyNodes sorted by key.
  static scoped_ptr<SchemaOwner> Wrap(const internal::SchemaNode* schema);

  // Parses the JSON schema in |schema| and returns a SchemaOwner that owns
  // the internal representation. If |schema| is invalid then NULL is returned
  // and |error| contains a reason for the failure.
  static scoped_ptr<SchemaOwner> Parse(const std::string& schema,
                                       std::string* error);

 private:
  explicit SchemaOwner(const internal::SchemaNode* root);

  // Parses the JSON schema in |schema| and returns the root SchemaNode if
  // successful, otherwise returns NULL. Any intermediate objects built by
  // this method are appended to the ScopedVectors.
  const internal::SchemaNode* Parse(const base::DictionaryValue& schema,
                                    std::string* error);

  // Helper for Parse().
  const internal::SchemaNode* ParseDictionary(
      const base::DictionaryValue& schema,
      std::string* error);

  // Helper for Parse().
  const internal::SchemaNode* ParseList(const base::DictionaryValue& schema,
                                        std::string* error);

  const internal::SchemaNode* root_;
  ScopedVector<internal::SchemaNode> schema_nodes_;
  // Note: |property_nodes_| contains PropertyNode[] elements and must be
  // cleared manually to properly use delete[].
  std::vector<internal::PropertyNode*> property_nodes_;
  ScopedVector<internal::PropertiesNode> properties_nodes_;
  ScopedVector<std::string> keys_;

  DISALLOW_COPY_AND_ASSIGN(SchemaOwner);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
