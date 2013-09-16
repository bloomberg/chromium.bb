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

struct POLICY_EXPORT SchemaNode {
  base::Value::Type type;

  // If |type| is TYPE_LIST then this is a SchemaNode* describing the
  // element type.
  //
  // If |type| is TYPE_DICTIONARY then this is a PropertiesNode* that can
  // contain any number of named properties and optionally a SchemaNode* for
  // additional properties.
  //
  // This is NULL if |type| has any other values.
  const void* extra;
};

struct POLICY_EXPORT PropertyNode {
  const char* key;
  const SchemaNode* schema;
};

struct POLICY_EXPORT PropertiesNode {
  const PropertyNode* begin;
  const PropertyNode* end;
  const SchemaNode* additional;
};

}  // namespace internal
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_SCHEMA_INTERNAL_H_
