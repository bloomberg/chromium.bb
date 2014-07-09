// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "ui/gfx/native_widget_types.h"

class Browser;

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}

namespace gfx {
class Size;
}

namespace extensions {

// Base class for platform-specific views used by extensions in the Chrome UI.
class ExtensionView {
 public:
  virtual ~ExtensionView() {}

  // Initialize the view, once a newly created view has been set in the host.
  virtual void Init() = 0;

  // If attached to a Browser (e.g. popups), the Browser it is attached to.
  virtual Browser* GetBrowser() = 0;

  // Returns the extension's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Method for the ExtensionHost to notify us about the correct size for
  // extension contents.
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) = 0;

  // Method for the ExtensionHost to notify us when the RenderViewHost has a
  // connection.
  virtual void RenderViewCreated() = 0;

  // Handles unhandled keyboard messages coming back from the renderer process.
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) = 0;

  // Method for the ExtensionHost to notify that the extension page has loaded.
  virtual void DidStopLoading() = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
