// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Browser;
class ExtensionHost;
class RenderViewHost;
class RenderWidgetHostViewGtk;
class SkBitmap;

class ExtensionViewGtk {
 public:
  ExtensionViewGtk(ExtensionHost* extension_host, Browser* browser);

  void Init();

  gfx::NativeView native_view();
  Browser* browser() const { return browser_; }

  bool is_toolstrip() const { return is_toolstrip_; }
  void set_is_toolstrip(bool is_toolstrip) { is_toolstrip_ = is_toolstrip; }

  void SetBackground(const SkBitmap& background);

  // Method for the ExtensionHost to notify us about the correct size for
  // extension contents.
  void UpdatePreferredSize(const gfx::Size& new_size);

  // Method for the ExtensionHost to notify us when the RenderViewHost has a
  // connection.
  void RenderViewCreated();

  RenderViewHost* render_view_host() const;

 private:
  void CreateWidgetHostView();

  // True if the contents are being displayed inside the extension shelf.
  bool is_toolstrip_;

  Browser* browser_;

  ExtensionHost* extension_host_;

  RenderWidgetHostViewGtk* render_widget_host_view_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewGtk);
};

#endif  // CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_
