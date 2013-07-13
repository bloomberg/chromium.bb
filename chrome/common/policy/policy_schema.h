// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_POLICY_POLICY_SCHEMA_H_
#define CHROME_COMMON_POLICY_POLICY_SCHEMA_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace policy {

class PolicySchema;
typedef std::map<std::string, PolicySchema*> PolicySchemaMap;

// Maps known policy keys to their expected types, and recursively describes
// the known keys within dictionary or list types.
class PolicySchema {
 public:

  // Parses |schema| as a JSON v3 schema, and additionally verifies that:
  // - the version is JSON schema v3;
  // - the top-level entry is of type "object";
  // - the top-level object doesn't contain "additionalProperties" nor
  //   "patternProperties";
  // - each "property" maps to a schema with one "type";
  // - the type "any" is not used.
  // If all the checks pass then the parsed PolicySchema is returned; otherwise
  // returns NULL.
  static scoped_ptr<PolicySchema> Parse(const std::string& schema,
                                        std::string* error);

  explicit PolicySchema(base::Value::Type type);
  virtual ~PolicySchema();

  // Returns the expected type for this policy. At the top-level PolicySchema
  // this is always TYPE_DICTIONARY.
  base::Value::Type type() const { return type_; }

  // It is invalid to call these methods when type() is not TYPE_DICTIONARY.
  //
  // GetProperties() returns a map of the known property names to their schemas;
  // the map is never NULL.
  // GetSchemaForAdditionalProperties() returns the schema that should be used
  // for keys not found in the map, and may be NULL.
  // GetSchemaForProperty() is a utility method that combines both, returning
  // the mapped schema if found in GetProperties(), otherwise returning
  // GetSchemaForAdditionalProperties().
  virtual const PolicySchemaMap* GetProperties() const;
  virtual const PolicySchema* GetSchemaForAdditionalProperties() const;
  const PolicySchema* GetSchemaForProperty(const std::string& key) const;

  // It is invalid to call this method when type() is not TYPE_LIST.
  // Returns the type of the entries of this "array", which is never NULL.
  virtual const PolicySchema* GetSchemaForItems() const;

 private:
  const base::Value::Type type_;

  DISALLOW_COPY_AND_ASSIGN(PolicySchema);
};

}  // namespace policy

#endif  // CHROME_COMMON_POLICY_POLICY_SCHEMA_H_
