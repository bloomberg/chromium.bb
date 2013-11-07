// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

struct POLICY_EXPORT SchemaData;
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

  // Builds a schema pointing to the inner structure of |data|,
  // rooted at |node|.
  Schema(const internal::SchemaData* data, const internal::SchemaNode* node);

  Schema(const Schema& schema);

  Schema& operator=(const Schema& schema);

  // Returns true if this Schema is valid. Schemas returned by the methods below
  // may be invalid, and in those cases the other methods must not be used.
  bool valid() const { return data_ != NULL; }

  base::Value::Type type() const;

  // Used to iterate over the known properties of TYPE_DICTIONARY schemas.
  class POLICY_EXPORT Iterator {
   public:
    Iterator(const internal::SchemaData* data,
             const internal::PropertiesNode* node);
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
    const internal::SchemaData* data_;
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
  const internal::SchemaData* data_;
  const internal::SchemaNode* node_;
};

// Owns schemas for policies of a given component.
class POLICY_EXPORT SchemaOwner {
 public:
  ~SchemaOwner();

  // The returned Schema is valid only during the lifetime of the SchemaOwner
  // that created it. It may be obtained multiple times.
  Schema schema() const;

  // Returns a SchemaOwner that references static data. This can be used by
  // the embedder to pass structures generated at compile time, which can then
  // be quickly loaded at runtime.
  static scoped_ptr<SchemaOwner> Wrap(const internal::SchemaData* data);

  // Parses the JSON schema in |schema| and returns a SchemaOwner that owns
  // the internal representation. If |schema| is invalid then NULL is returned
  // and |error| contains a reason for the failure.
  static scoped_ptr<SchemaOwner> Parse(const std::string& schema,
                                       std::string* error);

 private:
  class InternalStorage;

  SchemaOwner(const internal::SchemaData* data,
              scoped_ptr<InternalStorage> storage);

  // Holds the internal structures when a SchemaOwner is created via Parse().
  // SchemaOwners that Wrap() a SchemaData have a NULL storage.
  scoped_ptr<InternalStorage> storage_;
  const internal::SchemaData* data_;

  DISALLOW_COPY_AND_ASSIGN(SchemaOwner);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_H_
