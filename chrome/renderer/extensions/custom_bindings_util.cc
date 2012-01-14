// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/custom_bindings_util.h"

#include <map>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/context_menus_custom_bindings.h"
#include "chrome/renderer/extensions/experimental.socket_custom_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/page_actions_custom_bindings.h"
#include "chrome/renderer/extensions/tabs_custom_bindings.h"
#include "chrome/renderer/extensions/tts_custom_bindings.h"
#include "chrome/renderer/extensions/web_request_custom_bindings.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

namespace custom_bindings_util {

std::vector<v8::Extension*> GetAll(ExtensionDispatcher* extension_dispatcher) {
  // Must match kResourceIDs.
  static const char* kJavascriptFiles[] = {
    "extensions/browser_action_custom_bindings.js",
    "extensions/experimental.input.ime_custom_bindings.js",
    "extensions/omnibox_custom_bindings.js",
    "extensions/page_action_custom_bindings.js",
    "extensions/tts_engine_custom_bindings.js",
    "extensions/windows_custom_bindings.js",
  };
  static const size_t kJavascriptFilesSize = arraysize(kJavascriptFiles);

  // Must match kJavascriptFiles.
  static const int kResourceIDs[] = {
    IDR_BROWSER_ACTION_CUSTOM_BINDINGS_JS,
    IDR_EXPERIMENTAL_INPUT_IME_CUSTOM_BINDINGS_JS,
    IDR_OMNIBOX_CUSTOM_BINDINGS_JS,
    IDR_PAGE_ACTION_CUSTOM_BINDINGS_JS,
    IDR_TTS_ENGINE_CUSTOM_BINDINGS_JS,
    IDR_WINDOWS_CUSTOM_BINDINGS_JS,
  };
  static const size_t kResourceIDsSize = arraysize(kResourceIDs);

  static const char* kDependencies[] = {
    "extensions/schema_generated_bindings.js",
  };
  static const size_t kDependencyCount = arraysize(kDependencies);

  std::vector<v8::Extension*> result;

  // Custom bindings that have native code parts.
  result.push_back(new ContextMenusCustomBindings(
      kDependencyCount, kDependencies));
  result.push_back(new PageActionsCustomBindings(
      kDependencyCount, kDependencies, extension_dispatcher));
  result.push_back(new ExperimentalSocketCustomBindings(
      kDependencyCount, kDependencies));
  result.push_back(new TabsCustomBindings(
      kDependencyCount, kDependencies));
  result.push_back(new TTSCustomBindings(
      kDependencyCount, kDependencies));
  result.push_back(new WebRequestCustomBindings(
      kDependencyCount, kDependencies));

  // Pure JavaScript custom bindings.
  CHECK_EQ(kJavascriptFilesSize, kResourceIDsSize);
  for (size_t i = 0; i < kJavascriptFilesSize; ++i) {
    result.push_back(new ChromeV8Extension(
        kJavascriptFiles[i],
        kResourceIDs[i],
        kDependencyCount, kDependencies,
        NULL));
  }

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
                       const Extension& extension) {
  CHECK(api_name != "");

  // As in ExtensionAPI::GetSchemasForExtension, we need to allow any bindings
  // for an API that the extension *might* have permission to use.
  return extension.required_permission_set()->HasAnyAccessToAPI(api_name) ||
         extension.optional_permission_set()->HasAnyAccessToAPI(api_name);
}

}  // namespace custom_bindings_util

}  // namespace extensions
