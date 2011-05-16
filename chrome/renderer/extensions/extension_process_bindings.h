// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exposes extension APIs into the extension process.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "v8/include/v8.h"

class ExtensionDispatcher;
class GURL;
class URLPattern;

namespace WebKit {
class WebView;
}

class ExtensionProcessBindings {
 public:
  static v8::Extension* Get(ExtensionDispatcher* extension_dispatcher);

  // Handles a response to an API request.
  static void HandleResponse(int request_id, bool success,
                             const std::string& response,
                             const std::string& error);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_PROCESS_BINDINGS_H_
