// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exposes extension APIs into the extension process.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_

#include <map>
#include <string>
#include <vector>

#include "v8/include/v8.h"

class ExtensionProcessBindings {
 public:
  static void SetFunctionNames(const std::vector<std::string>& names);
  static v8::Extension* Get();

  // Handles a response to an API request.
  static void HandleResponse(int request_id, bool success,
                             const std::string& response,
                             const std::string& error);

  // Sets the page action ids for a particular extension.
  static void SetPageActions(const std::string& extension_id,
                             const std::vector<std::string>& page_actions);

  // Sets the permissions for a particular extension.
  static void SetPermissions(const std::string& extension_id,
                             const std::vector<std::string>& permissions);

  // Check if the extension in the currently running context has permission to
  // access the given extension function. Must be called with a valid V8
  // context in scope.
  static bool CurrentContextHasPermission(const std::string& function_name);

  // Throw a V8 exception indicating that permission to access function_name was
  // denied. Must be called with a valid V8 context in scope.
  static v8::Handle<v8::Value> ThrowPermissionDeniedException(
      const std::string& function_name);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
