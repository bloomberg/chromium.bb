// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class Browser;
class ExtensionHost;
class RenderViewHost;
class RenderWidgetHostViewMac;
class SkBitmap;

// This class represents extension views. An extension view internally contains
// a bridge to an extension process, which draws to the extension view's
// native view object through IPC.
class ExtensionViewMac {
 public:
  ExtensionViewMac(ExtensionHost* extension_host, Browser* browser);
  ~ExtensionViewMac();

  // Starts the extension process and creates the native view. You must call
  // this method before calling any of this class's other methods.
  void Init();

  // Returns the extension's native view.
  gfx::NativeView native_view();

  // Returns the browser the extension belongs to.
  Browser* browser() const { return browser_; }

  // Does this extension live as a toolstrip in an extension shelf?
  bool is_toolstrip() const { return is_toolstrip_; }
  void set_is_toolstrip(bool is_toolstrip) { is_toolstrip_ = is_toolstrip; }

  // Sets the extensions's background image.
  void SetBackground(const SkBitmap& background);

  // Method for the ExtensionHost to notify us about the correct size for
  // extension contents.
  void UpdatePreferredSize(const gfx::Size& new_size);

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
  RenderViewHost* render_view_host() const;

  void CreateWidgetHostView();

  // True if the contents are being displayed inside the extension shelf.
  bool is_toolstrip_;

  Browser* browser_;  // weak

  ExtensionHost* extension_host_;  // weak

  // Created by us, but owned by its |native_view()|. We |release| the
  // rwhv's native view in our destructor, effectively freeing this.
  RenderWidgetHostViewMac* render_widget_host_view_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
