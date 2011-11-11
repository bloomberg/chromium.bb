// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The AppObjectExtension is a v8 extension that creates an object
// at window.chrome.app.  This object allows javascript to get details
// on the app state of the page.
// The read-only property app.isInstalled is true if the current page is
// within the extent of an installed, enabled app.

#ifndef CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"

class ChromeV8Context;

// Implements the chrome.app JavaScript object.
//
// TODO(aa): Add unit testing for this class.
class AppBindings : public ChromeV8Extension {
 public:
  explicit AppBindings(ExtensionDispatcher* dispatcher);

 protected:
  virtual ChromeV8ExtensionHandler* CreateHandler(
      ChromeV8Context* context) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppBindings);
};

#endif  // CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_
