// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_BINDING_TEST_UTIL_H_
#define EXTENSIONS_RENDERER_API_BINDING_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "v8/include/v8.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace extensions {

// Returns a string with all single quotes replaced with double quotes. Useful
// to write JSON strings without needing to escape quotes.
std::string ReplaceSingleQuotes(base::StringPiece str);

// Returns a base::Value parsed from |str|. EXPECTs the conversion to succeed.
std::unique_ptr<base::Value> ValueFromString(base::StringPiece str);

// As above, but returning a ListValue.
std::unique_ptr<base::ListValue> ListValueFromString(base::StringPiece str);

// As above, but returning a DictionaryValue.
std::unique_ptr<base::DictionaryValue> DictionaryValueFromString(
    base::StringPiece str);

// Converts the given |value| to a JSON string. EXPECTs the conversion to
// succeed.
std::string ValueToString(const base::Value& value);

// Returns a v8::Value result from compiling and running |source|, or an empty
// local on failure.
v8::Local<v8::Value> V8ValueFromScriptSource(v8::Local<v8::Context> context,
                                             base::StringPiece source);

// Returns a v8::Function parsed from the given |source|. EXPECTs the conversion
// to succeed.
v8::Local<v8::Function> FunctionFromString(v8::Local<v8::Context> context,
                                           base::StringPiece source);

// Converts the given |value| to a base::Value and returns the result.
std::unique_ptr<base::Value> V8ToBaseValue(v8::Local<v8::Value> value,
                                           v8::Local<v8::Context> context);

}  // extensions

#endif  // EXTENSIONS_RENDERER_API_BINDING_TEST_UTIL_H_
