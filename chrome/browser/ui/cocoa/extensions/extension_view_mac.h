// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class Browser;
class SkBitmap;

namespace content {
class RenderViewHost;
}

namespace extensions {
class ExtensionHost;
}

// This class represents extension views. An extension view internally contains
// a bridge to an extension process, which draws to the extension view's
// native view object through IPC.
class ExtensionViewMac {
 public:
  class Container {
   public:
    virtual ~Container() {}
    virtual void OnExtensionSizeChanged(ExtensionViewMac* view,
                                        const gfx::Size& new_size) {}
    virtual void OnExtensionViewDidShow(ExtensionViewMac* view) {};
  };

  ExtensionViewMac(extensions::ExtensionHost* extension_host, Browser* browser);
  ~ExtensionViewMac();

  // Starts the extension process and creates the native view. You must call
  // this method before calling any of this class's other methods.
  void Init();

  // Returns the extension's native view.
  gfx::NativeView native_view();

  // Returns the browser the extension belongs to.
  Browser* browser() const { return browser_; }

  // Method for the ExtensionHost to notify us that the extension page is
  // loaded.
  void DidStopLoading();

  // Sets the container for this view.
  void set_container(Container* container) { container_ = container; }

  // Method for the ExtensionHost to notify us about the correct size for
  // extension contents.
  void ResizeDueToAutoResize(const gfx::Size& new_size);

  // Method for the ExtensionHost to notify us when the RenderViewHost has a
  // connection.
  void RenderViewCreated();

  // Informs the view that its containing window's frame changed.
  void WindowFrameChanged();

  // The minimum/maximum dimensions of the popup.
  // The minimum is just a little larger than the size of the button itself.
  // The maximum is an arbitrary number that should be smaller than most
  // screens.
  static const CGFloat kMinWidth;
  static const CGFloat kMinHeight;
  static const CGFloat kMaxWidth;
  static const CGFloat kMaxHeight;

 private:
  content::RenderViewHost* render_view_host() const;

  void CreateWidgetHostView();

  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  Browser* browser_;  // weak

  extensions::ExtensionHost* extension_host_;  // weak

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  gfx::Size pending_preferred_size_;

  Container* container_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
