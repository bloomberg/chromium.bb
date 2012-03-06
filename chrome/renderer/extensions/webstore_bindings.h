// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"

class ChromeV8Context;

// A V8 extension that creates an object at window.chrome.webstore. This object
// allows JavaScript to initiate inline installs of apps that are listed in the
// Chrome Web Store (CWS).
class WebstoreBindings : public ChromeV8Extension {
 public:
  explicit WebstoreBindings(ExtensionDispatcher* dispatcher);

 protected:
  virtual ChromeV8ExtensionHandler* CreateHandler(
      ChromeV8Context* context) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebstoreBindings);
};

#endif  // CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
