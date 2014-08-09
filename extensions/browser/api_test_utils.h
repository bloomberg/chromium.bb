// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_TEST_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"

class UIThreadExtensionFunction;

namespace base {
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionFunctionDispatcher;

// TODO(yoz): crbug.com/394840: Remove duplicate functionality in
// chrome/browser/extensions/extension_function_test_utils.h.
namespace api_test_utils {

enum RunFunctionFlags { NONE = 0, INCLUDE_INCOGNITO = 1 << 0 };

// Run |function| with |args| and return the result. Adds an error to the
// current test if |function| returns an error. Takes ownership of
// |function|. The caller takes ownership of the result.
base::Value* RunFunctionWithDelegateAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    scoped_ptr<ExtensionFunctionDispatcher> dispatcher);
base::Value* RunFunctionWithDelegateAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    scoped_ptr<ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags);

// RunFunctionWithDelegateAndReturnSingleResult, except with a NULL
// implementation of the Delegate.
base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context);
base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    RunFunctionFlags flags);

// Create and run |function| with |args|. Works with both synchronous and async
// functions. Ownership of |function| remains with the caller.
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
                 content::BrowserContext* context,
                 scoped_ptr<ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags);

}  // namespace function_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_UTILS_H_
