// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SCHEMA_GENERATED_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_SCHEMA_GENERATED_BINDINGS_H_
#pragma once

#include <string>

class ExtensionDispatcher;
class ChromeV8ContextSet;

namespace v8 {
class Extension;
}

namespace extensions {

// Generates JavaScript bindings for the extension system from the declarations
// in the extension_api.json file.
class SchemaGeneratedBindings {
 public:
  static v8::Extension* Get(ExtensionDispatcher* extension_dispatcher);

  // Handles a response to an API request.  Sets |extension_id|.
  static void HandleResponse(const ChromeV8ContextSet& contexts,
                             int request_id,
                             bool success,
                             const std::string& response,
                             const std::string& error,
                             std::string* extension_id);

  static bool HasPendingRequests(const std::string& extension_id);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SCHEMA_GENERATED_BINDINGS_H_
