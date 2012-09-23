// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

// This class represents extension views. An extension view internally contains
// a bridge to an extension process, which draws to the extension view's
// native view object through IPC.
class ExtensionViewMac : public ExtensionView {
 public:
  ExtensionViewMac(extensions::ExtensionHost* extension_host, Browser* browser);
  virtual ~ExtensionViewMac();

  // Starts the extension process and creates the native view. You must call
  // this method before calling any of this class's other methods.
  void Init();

  // Sets the extensions's background image.
  void SetBackground(const SkBitmap& background);

  // The minimum/maximum dimensions of the popup.
  // The minimum is just a little larger than the size of the button itself.
  // The maximum is an arbitrary number that should be smaller than most
  // screens.
  static const CGFloat kMinWidth;
  static const CGFloat kMinHeight;
  static const CGFloat kMaxWidth;
  static const CGFloat kMaxHeight;

 private:
  // Overridden from ExtensionView:
  virtual Browser* GetBrowser() OVERRIDE;
  virtual const Browser* GetBrowser() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual void SetContainer(ExtensionViewContainer* container) OVERRIDE;
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) OVERRIDE;
  virtual void RenderViewCreated() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void WindowFrameChanged() OVERRIDE;

  void CreateWidgetHostView();

  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  Browser* browser_;  // weak

  extensions::ExtensionHost* extension_host_;  // weak

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  gfx::Size pending_preferred_size_;

  ExtensionViewContainer* container_;  // weak

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_VIEW_MAC_H_
