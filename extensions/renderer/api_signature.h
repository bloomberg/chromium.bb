// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_SIGNATURE_H_
#define EXTENSIONS_RENDERER_API_SIGNATURE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "extensions/renderer/argument_spec.h"
#include "v8/include/v8.h"

namespace base {
class Value;
class ListValue;
}

namespace gin {
class Arguments;
}

namespace extensions {

// A representation of the expected signature for an API method, along with the
// ability to match provided arguments and convert them to base::Values.
class APISignature {
 public:
  APISignature(const base::ListValue& specification);
  ~APISignature();

  // Parses |arguments| against this signature, converting to a base::ListValue.
  // If the arguments don't match, null is returned and |error| is populated.
  std::unique_ptr<base::ListValue> ParseArguments(
      gin::Arguments* arguments,
      const ArgumentSpec::RefMap& type_refs,
      v8::Local<v8::Function>* callback_out,
      std::string* error) const;

 private:
  // Attempts to match an argument from |arguments| to the given |spec|.
  // If the next argmument does not match and |spec| is optional, a null
  // base::Value is returned.
  // If the argument matches, |arguments| is advanced and the converted value is
  // returned.
  // If the argument does not match and it is not optional, returns null and
  // populates error.
  std::unique_ptr<base::Value> ParseArgument(
      const ArgumentSpec& spec,
      v8::Local<v8::Context> context,
      gin::Arguments* arguments,
      const ArgumentSpec::RefMap& type_refs,
      std::string* error) const;

  // Parses the callback from |arguments| according to |callback_spec|. Since
  // the callback isn't converted into a base::Value, this is different from
  // ParseArgument() above.
  bool ParseCallback(gin::Arguments* arguments,
                     const ArgumentSpec& callback_spec,
                     std::string* error,
                     v8::Local<v8::Function>* callback_out) const;

  // The list of expected arguments.
  std::vector<std::unique_ptr<ArgumentSpec>> signature_;

  DISALLOW_COPY_AND_ASSIGN(APISignature);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_SIGNATURE_H_
