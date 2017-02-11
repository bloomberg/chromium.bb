// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_SIGNATURE_H_
#define EXTENSIONS_RENDERER_API_SIGNATURE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "v8/include/v8.h"

namespace base {
class Value;
class ListValue;
}

namespace extensions {
class APITypeReferenceMap;
class ArgumentSpec;

// A representation of the expected signature for an API method, along with the
// ability to match provided arguments and convert them to base::Values.
class APISignature {
 public:
  APISignature(const base::ListValue& specification);
  ~APISignature();

  // Parses |arguments| against this signature, and populates |args_out| with
  // the v8 values (performing no conversion). The resulting vector may differ
  // from the list of arguments passed in because it will include null-filled
  // optional arguments.
  // Returns true if the arguments were successfully parsed and converted.
  bool ParseArgumentsToV8(v8::Local<v8::Context> context,
                          const std::vector<v8::Local<v8::Value>>& arguments,
                          const APITypeReferenceMap& type_refs,
                          std::vector<v8::Local<v8::Value>>* args_out,
                          std::string* error) const;

  // Parses |arguments| against this signature, converting to a base::ListValue.
  // Returns true if the arguments were successfully parsed and converted, and
  // populates |args_out| and |callback_out| with the JSON arguments and
  // callback values, respectively. On failure, returns false populates |error|.
  bool ParseArgumentsToJSON(v8::Local<v8::Context> context,
                            const std::vector<v8::Local<v8::Value>>& arguments,
                            const APITypeReferenceMap& type_refs,
                            std::unique_ptr<base::ListValue>* args_out,
                            v8::Local<v8::Function>* callback_out,
                            std::string* error) const;

 private:
  // The list of expected arguments.
  std::vector<std::unique_ptr<ArgumentSpec>> signature_;

  DISALLOW_COPY_AND_ASSIGN(APISignature);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_SIGNATURE_H_
