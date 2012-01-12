// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/custom_bindings_util.h"

#include <map>

#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/contextMenus_custom_bindings.h"
#include "base/logging.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

namespace custom_bindings_util {

std::vector<v8::Extension*> GetAll() {
  static const char* kDependencies[] = {
    "extensions/schema_generated_bindings.js",
  };
  static const size_t kDependencyCount = arraysize(kDependencies);

  std::vector<v8::Extension*> result;

  result.push_back(
      new ContextMenusCustomBindings(kDependencyCount, kDependencies));

  return result;
}

// Extracts the name of an API from the name of the V8 extension which contains
// custom bindings for it (see kCustomBindingNames).
std::string GetAPIName(const std::string& v8_extension_name) {
  // Extract the name of the API from the v8 extension name.
  // This is "${api_name}" in "extensions/${api_name}_custom_bindings.js".
  std::string prefix = "extensions/";
  const bool kCaseSensitive = true;
  if (!StartsWithASCII(v8_extension_name, prefix, kCaseSensitive))
    return "";

  std::string suffix = "_custom_bindings.js";
  if (!EndsWith(v8_extension_name, suffix, kCaseSensitive))
    return "";

  return v8_extension_name.substr(
      prefix.size(),
      v8_extension_name.size() - prefix.size() - suffix.size());
}

bool AllowAPIInjection(const std::string& api_name,
                       const Extension& extension) {
  CHECK(api_name != "");

  // As in ExtensionAPI::GetSchemasForExtension, we need to allow any bindings
  // for an API that the extension *might* have permission to use.
  if (!extension.required_permission_set()->HasAnyAccessToAPI(api_name) &&
      !extension.optional_permission_set()->HasAnyAccessToAPI(api_name)) {
    return false;
  }

  return true;
}

}  // namespace custom_bindings_util

}  // namespace extensions
