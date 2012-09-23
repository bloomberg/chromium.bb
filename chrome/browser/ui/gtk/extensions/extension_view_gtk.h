// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_VIEW_GTK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class RenderViewHost;
}

namespace extensions {
class ExtensionHost;
}

class ExtensionViewGtk : public ExtensionView {
 public:
  ExtensionViewGtk(extensions::ExtensionHost* extension_host, Browser* browser);
  virtual ~ExtensionViewGtk();

  void Init();

  void SetBackground(const SkBitmap& background);

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

  Browser* browser_;

  extensions::ExtensionHost* extension_host_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // This view's container.
  ExtensionViewContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_VIEW_GTK_H_
