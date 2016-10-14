// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ARGUMENT_SPEC_H_
#define EXTENSIONS_RENDERER_ARGUMENT_SPEC_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "v8/include/v8.h"

namespace base {
class Value;
}

namespace extensions {

enum class ArgumentType {
  INTEGER,
  DOUBLE,
  BOOLEAN,
  STRING,
  OBJECT,
  LIST,
  FUNCTION,
  ANY,
};

// A description of a given Argument to an Extension.
class ArgumentSpec {
 public:
  // Reads the description from |value| and sets associated fields.
  // TODO(devlin): We should strongly think about generating these instead of
  // populating them at runtime.
  explicit ArgumentSpec(const base::Value& value);
  ~ArgumentSpec();

  // Returns the converted base::Value or null if the |value| didn't match.
  std::unique_ptr<base::Value> ConvertArgument(v8::Local<v8::Context> context,
                                               v8::Local<v8::Value> value,
                                               std::string* error) const;

  const std::string& name() const { return name_; }
  bool optional() const { return optional_; }
  ArgumentType type() const { return type_; }

 private:
  // Returns true if this argument refers to a fundamental type.
  bool IsFundamentalType() const;

  // Conversion functions. These should only be used if the spec is of the given
  // type (otherwise, they will DCHECK).
  std::unique_ptr<base::Value> ConvertArgumentToFundamental(
      v8::Local<v8::Context> context,
      v8::Local<v8::Value> value,
      std::string* error) const;
  std::unique_ptr<base::Value> ConvertArgumentToObject(
      v8::Local<v8::Context> context,
      v8::Local<v8::Object> object,
      std::string* error) const;
  std::unique_ptr<base::Value> ConvertArgumentToArray(
      v8::Local<v8::Context> context,
      v8::Local<v8::Array> value,
      std::string* error) const;
  std::unique_ptr<base::Value> ConvertArgumentToAny(
      v8::Local<v8::Context> context,
      v8::Local<v8::Value> value,
      std::string* error) const;

  // The name of the argument.
  std::string name_;

  // The type of the argument.
  ArgumentType type_;

  // Whether or not the argument is required.
  bool optional_;

  // A minimum, if any.
  base::Optional<int> minimum_;

  // A map of required properties; present only for objects. Note that any
  // properties *not* defined in this map will be dropped during conversion.
  std::map<std::string, std::unique_ptr<ArgumentSpec>> properties_;

  // The type of item that should be in the list; present only for lists.
  std::unique_ptr<ArgumentSpec> list_element_type_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentSpec);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ARGUMENT_SPEC_H_
