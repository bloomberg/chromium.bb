// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ChromeStoreExtension is a V8 extension that creates an object at
// window.chrome.webstore.  This object allows JavaScript to initiate inline
// installs of apps that are listed in the Chrome Web Store (CWS).

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_WEBSTORE_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_WEBSTORE_BINDINGS_H_
#pragma once

#include <string>

namespace v8 {
class Extension;
}

class ChromeWebstoreExtension {
 public:
  static v8::Extension* Get();

  static void HandleInstallResponse(int install_id,
                                    bool success,
                                    const std::string& error);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_WEBSTORE_BINDINGS_H_
