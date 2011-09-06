// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The AppObjectExtension is a v8 extension that creates an object
// at window.chrome.app.  This object allows javascript to get details
// on the app state of the page.
// The read-only property app.isInstalled is true if the current page is
// within the extent of an installed, enabled app.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_APP_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_APP_BINDINGS_H_
#pragma once

class ExtensionRendererContext;

namespace v8 {
class Extension;
}

namespace extensions_v8 {

class ChromeAppExtension {
 public:
  static v8::Extension* Get(
      ExtensionRendererContext* extension_renderer_context);
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_APP_BINDINGS_H_
