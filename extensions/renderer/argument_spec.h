// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_ARGUMENT_SPEC_H_
#define EXTENSIONS_RENDERER_ARGUMENT_SPEC_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "v8/include/v8.h"

namespace base {
class DictionaryValue;
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
  REF,
  CHOICES,
};

// A description of a given Argument to an Extension.
class ArgumentSpec {
 public:
  // A map from name -> definition for type definitions. This is used when an
  // argument is declared to be a reference to a type defined elsewhere.
  using RefMap = std::map<std::string, std::unique_ptr<ArgumentSpec>>;

  // Reads the description from |value| and sets associated fields.
  // TODO(devlin): We should strongly think about generating these instead of
  // populating them at runtime.
  explicit ArgumentSpec(const base::Value& value);
  ~ArgumentSpec();

  // Returns true if the passed |value| matches this specification. If
  // |out_value| is non-null, converts the value to a base::Value and populates
  // |out_value|. Otherwise, no conversion is performed.
  bool ParseArgument(v8::Local<v8::Context> context,
                     v8::Local<v8::Value> value,
                     const RefMap& refs,
                     std::unique_ptr<base::Value>* out_value,
                     std::string* error) const;

  const std::string& name() const { return name_; }
  bool optional() const { return optional_; }
  ArgumentType type() const { return type_; }
  const std::set<std::string>& enum_values() const { return enum_values_; }

 private:
  // Initializes this object according to |type_string| and |dict|.
  void InitializeType(const base::DictionaryValue* dict);

  // Returns true if this argument refers to a fundamental type.
  bool IsFundamentalType() const;

  // Conversion functions. These should only be used if the spec is of the given
  // type (otherwise, they will DCHECK).
  bool ParseArgumentToFundamental(v8::Local<v8::Context> context,
                                  v8::Local<v8::Value> value,
                                  std::unique_ptr<base::Value>* out_value,
                                  std::string* error) const;
  bool ParseArgumentToObject(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> object,
                             const RefMap& refs,
                             std::unique_ptr<base::Value>* out_value,
                             std::string* error) const;
  bool ParseArgumentToArray(v8::Local<v8::Context> context,
                            v8::Local<v8::Array> value,
                            const RefMap& refs,
                            std::unique_ptr<base::Value>* out_value,
                            std::string* error) const;
  bool ParseArgumentToAny(v8::Local<v8::Context> context,
                          v8::Local<v8::Value> value,
                          std::unique_ptr<base::Value>* out_value,
                          std::string* error) const;

  // The name of the argument.
  std::string name_;

  // The type of the argument.
  ArgumentType type_;

  // Whether or not the argument is required.
  bool optional_;

  // The reference the argument points to, if any. Note that if this is set,
  // none of the following fields describing the argument will be.
  base::Optional<std::string> ref_;

  // A minimum, if any.
  base::Optional<int> minimum_;

  // A map of required properties; present only for objects. Note that any
  // properties *not* defined in this map will be dropped during conversion.
  std::map<std::string, std::unique_ptr<ArgumentSpec>> properties_;

  // The type of item that should be in the list; present only for lists.
  std::unique_ptr<ArgumentSpec> list_element_type_;

  // The different possible specs this argument can map to. Only populated for
  // arguments of type CHOICES.
  std::vector<std::unique_ptr<ArgumentSpec>> choices_;

  // The possible enum values, if defined for this argument.
  std::set<std::string> enum_values_;

  DISALLOW_COPY_AND_ASSIGN(ArgumentSpec);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_ARGUMENT_SPEC_H_
