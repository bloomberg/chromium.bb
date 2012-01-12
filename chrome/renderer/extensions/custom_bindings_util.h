// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CUSTOM_BINDINGS_UTIL_H_
#define CHROME_RENDERER_EXTENSIONS_CUSTOM_BINDINGS_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/renderer/extensions/chrome_v8_extension.h"

class Extension;
class ExtensionDispatcher;

namespace v8 {
class Extension;
}

namespace extensions {

// Utilities for managing the set of V8 extensions for extension API custom
// bindings.
namespace custom_bindings_util {

// Creates V8 extensions for all custom bindings.
std::vector<v8::Extension*> GetAll(ExtensionDispatcher* extension_dispatcher);

// Extracts the name of an API from the name of the V8 extension which contains
// custom bindings for it.
// Returns an empty string if the extension is not for a custom binding.
std::string GetAPIName(const std::string& v8_extension_name);

// Returns whether the custom binding for an API should be allowed to run for
// |extension|.  This is based on whether the extension has any permission
// (required or optional) for that API.
bool AllowAPIInjection(const std::string& api_name,
                       const Extension& extension);

}  // namespace custom_bindings_util

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CUSTOM_BINDINGS_UTIL_H_
