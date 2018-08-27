// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JSON_SCHEMA_JSON_SCHEMA_VALIDATOR_H_
#define COMPONENTS_JSON_SCHEMA_JSON_SCHEMA_VALIDATOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
class DictionaryValue;
}

//==============================================================================
// This class implements a subset of JSON Schema.
// See: http://www.json.com/json-schema-proposal/ for more details.
//
// There is also an older JavaScript implementation of the same functionality in
// chrome/renderer/resources/json_schema.js.
//
// The following features of JSON Schema are not implemented:
// - requires
// - unique
// - disallow
// - union types (but replaced with 'choices')
// - number.maxDecimal
//
// The following properties are not applicable to the interface exposed by
// this class:
// - options
// - readonly
// - title
// - description
// - format
// - default
// - transient
// - hidden
//
// There are also these departures from the JSON Schema proposal:
// - null counts as 'unspecified' for optional values
// - added the 'choices' property, to allow specifying a list of possible types
//   for a value
// - by default an "object" typed schema does not allow additional properties.
//   if present, "additionalProperties" is to be a schema against which all
//   additional properties will be validated.
// - regular expression supports all syntaxes that re2 accepts.
//   See https://github.com/google/re2/blob/master/doc/syntax.txt for details.
//==============================================================================
class JSONSchemaValidator {
 public:
  enum Options {
    // Ignore unknown attributes. If this option is not set then unknown
    // attributes will make the schema validation fail.
    OPTIONS_IGNORE_UNKNOWN_ATTRIBUTES = 1 << 0,
  };

  // Verifies if |schema| is a valid JSON v3 schema. When this validation passes
  // then |schema| is valid JSON that can be parsed into a DictionaryValue,
  // and that DictionaryValue can be used to build a JSONSchemaValidator.
  // Returns the parsed DictionaryValue when |schema| validated, otherwise
  // returns NULL. In that case, |error| contains an error description.
  // For performance reasons, currently IsValidSchema() won't check the
  // correctness of regular expressions used in "pattern" and
  // "patternProperties" and in Validate() invalid regular expression don't
  // accept any strings.
  static std::unique_ptr<base::DictionaryValue> IsValidSchema(
      const std::string& schema,
      std::string* error);

  // Same as above but with |options|, which is a bitwise-OR combination of the
  // Options above.
  static std::unique_ptr<base::DictionaryValue>
  IsValidSchema(const std::string& schema, int options, std::string* error);

  DISALLOW_COPY_AND_ASSIGN(JSONSchemaValidator);
};

#endif  // COMPONENTS_JSON_SCHEMA_JSON_SCHEMA_VALIDATOR_H_
