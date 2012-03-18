// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/custom_bindings_util.h"

#include <map>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/chrome_private_custom_bindings.h"
#include "chrome/renderer/extensions/context_menus_custom_bindings.h"
#include "chrome/renderer/extensions/experimental.socket_custom_bindings.h"
#include "chrome/renderer/extensions/extension_custom_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"
#include "chrome/renderer/extensions/i18n_custom_bindings.h"
#include "chrome/renderer/extensions/page_actions_custom_bindings.h"
#include "chrome/renderer/extensions/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/tabs_custom_bindings.h"
#include "chrome/renderer/extensions/tts_custom_bindings.h"
#include "chrome/renderer/extensions/web_request_custom_bindings.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

namespace custom_bindings_util {

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

  // By convention, filenames are use unix_hacker_style, but the APIs we expose
  // to javascript use camelCase.
  std::string not_camelcase = v8_extension_name.substr(
      prefix.size(),
      v8_extension_name.size() - prefix.size() - suffix.size());

  std::string camelcase;
  bool next_to_upper = false;
  for (std::string::iterator it = not_camelcase.begin();
      it != not_camelcase.end(); ++it) {
    if (*it == '_') {
      next_to_upper = true;
    } else if (next_to_upper) {
      camelcase.push_back(base::ToUpperASCII(*it));
      next_to_upper = false;
    } else {
      camelcase.push_back(*it);
    }
  }

  return camelcase;
}

bool AllowAPIInjection(const std::string& api_name,
                       const Extension& extension,
                       ExtensionDispatcher* extension_dispatcher) {
  CHECK(api_name != "");

  // As in ExtensionAPI::GetSchemasForExtension, we need to allow any bindings
  // for an API that the extension *might* have permission to use.
  bool allowed =
      extension.required_permission_set()->HasAnyAccessToAPI(api_name) ||
      extension.optional_permission_set()->HasAnyAccessToAPI(api_name);

  if (extension_dispatcher->is_extension_process()) {
    return allowed;
  } else {
    return allowed &&
           !ExtensionAPI::GetInstance()->IsWholeAPIPrivileged(api_name);
  }
}

}  // namespace custom_bindings_util

}  // namespace extensions
