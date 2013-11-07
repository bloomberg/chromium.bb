// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_
#define COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_

#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {
namespace internal {

// These types are used internally by the SchemaOwner parser, and by the
// compile-time code generator. They shouldn't be used directly.

// Represents the type of one policy, or an item of a list policy, or a
// property of a map policy.
struct POLICY_EXPORT SchemaNode {
  // The policy type.
  base::Value::Type type;

  // If |type| is TYPE_DICTIONARY then |extra| is an offset into
  // SchemaData::properties_nodes that indexes the PropertiesNode describing
  // the entries of this dictionary.
  //
  // If |type| is TYPE_LIST then |extra| is an offset into
  // SchemaData::schema_nodes that indexes the SchemaNode describing the items
  // of this list.
  //
  // Otherwise extra is -1 and is invalid.
  int extra;
};

// Represents an entry of a map policy.
struct POLICY_EXPORT PropertyNode {
  // The entry key.
  const char* key;

  // An offset into SchemaData::schema_nodes that indexes the SchemaNode
  // describing the structure of this key.
  int schema;
};

// Represents the list of keys of a map policy.
struct POLICY_EXPORT PropertiesNode {
  // An offset into SchemaData::property_nodes that indexes the PropertyNode
  // describing the first known property of this map policy.
  int begin;

  // An offset into SchemaData::property_nodes that indexes the PropertyNode
  // right beyond the last known property of this map policy.
  //
  // If |begin == end| then the map policy that this PropertiesNode corresponds
  // to does not have known properties.
  //
  // Note that the range [begin, end) is sorted by PropertyNode::key, so that
  // properties can be looked up by binary searching in the range.
  int end;

  // If this map policy supports keys with any value (besides the well-known
  // values described in the range [begin, end)) then |additional| is an offset
  // into SchemaData::schema_nodes that indexes the SchemaNode describing the
  // structure of the values for those keys. Otherwise |additional| is -1 and
  // is invalid.
  int additional;
};

// Contains arrays of related nodes. All of the offsets in these nodes reference
// other nodes in these arrays.
struct POLICY_EXPORT SchemaData {
  const SchemaNode* schema_nodes;
  const PropertyNode* property_nodes;
  const PropertiesNode* properties_nodes;
};

}  // namespace internal
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_
