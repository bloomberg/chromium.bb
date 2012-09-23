// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_

#include "ui/gfx/native_widget_types.h"

class Browser;
class ExtensionViewContainer;

namespace content {
class RenderViewHost;
struct NativeWebKeyboardEvent;
}

namespace extensions {
class ExtensionHost;
}

namespace gfx {
class Size;
}

// This is a cross platform interface for extension view, and it's owned by
// ExtensionHost.
class ExtensionView {
 public:
  static ExtensionView* Create(extensions::ExtensionHost* host,
                               Browser* browser);

  virtual ~ExtensionView() {}

  // Returns the browser the extension belongs to.
  virtual Browser* GetBrowser() = 0;
  virtual const Browser* GetBrowser() const = 0;

  // Returns the extension's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Returns the render view host for this extension view.
  virtual content::RenderViewHost* GetRenderViewHost() const = 0;

  // Sets the container for this view.
  virtual void SetContainer(ExtensionViewContainer* container) = 0;

  // Used by ExtensionHost to notify the platform-specific implementations about
  // the correct size for extension contents.
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) = 0;

  // Used by ExtensionHost to notify the platform-specific implementations when
  // the RenderViewHost has a connection.
  virtual void RenderViewCreated() = 0;

  // Used by ExtensionHost to notify the platform-specific implementations that/
  // the extension page is loaded.
  virtual void DidStopLoading() = 0;

  // Informs the view that its containing window's frame changed.
  virtual void WindowFrameChanged() = 0;

  // Handles unhandled keyboard messages coming back from the renderer process.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) {}
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_H_
