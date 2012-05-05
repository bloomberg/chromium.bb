// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_TEST_UTILS_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension.h"

class AsyncExtensionFunction;
class Browser;
class Extension;
class UIThreadExtensionFunction;

namespace base {
class Value;
class DictionaryValue;
class ListValue;
}

namespace extension_function_test_utils {

// Parse JSON and return as the specified type, or NULL if the JSON is invalid
// or not the specified type.
base::Value* ParseJSON(const std::string& data);
base::ListValue* ParseList(const std::string& data);
base::DictionaryValue* ParseDictionary(const std::string& data);

// Get |key| from |val| as the specified type. If |key| does not exist, or is
// not of the specified type, adds a failure to the current test and returns
// false, 0, empty string, etc.
bool GetBoolean(base::DictionaryValue* val, const std::string& key);
int GetInteger(base::DictionaryValue* val, const std::string& key);
std::string GetString(base::DictionaryValue* val, const std::string& key);

// If |val| is a dictionary, return it as one, otherwise NULL.
base::DictionaryValue* ToDictionary(base::Value* val);

// If |val| is a list, return it as one, otherwise NULL.
base::ListValue* ToList(base::Value* val);

// Creates an extension instance that can be attached to an ExtensionFunction
// before running it.
scoped_refptr<Extension> CreateEmptyExtension();

// Creates an extension instance with a specified location that can be attached
// to an ExtensionFunction before running.
scoped_refptr<Extension> CreateEmptyExtensionWithLocation(
    Extension::Location location);

enum RunFunctionFlags {
  NONE = 0,
  INCLUDE_INCOGNITO = 1 << 0
};

// Run |function| with |args| and return the resulting error. Adds an error to
// the current test if |function| returns a result.
std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser,
                                      RunFunctionFlags flags);
std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser);

// Run |function| with |args| and return the result. Adds an error to the
// current test if |function| returns an error. The caller takes ownership of
// the result.
base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                        const std::string& args,
                                        Browser* browser,
                                        RunFunctionFlags flags);
base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                        const std::string& args,
                                        Browser* browser);

// Create and run |function| with |args|. Works with both synchronous and async
// functions.
//
// TODO(aa): It would be nice if |args| could be validated against the schema
// that |function| expects. That way, we know that we are testing something
// close to what the bindings would actually send.
//
// TODO(aa): I'm concerned that this style won't scale to all the bits and bobs
// we're going to need to frob for all the different extension functions. But
// we can refactor when we see what is needed.
bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 RunFunctionFlags flags);

} // namespace extension_function_test_utils

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_TEST_UTILS_H_
