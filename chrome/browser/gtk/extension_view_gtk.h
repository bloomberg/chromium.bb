// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_

#include "base/basictypes.h"
#include "base/gfx/native_widget_types.h"

class Browser;
class ExtensionHost;
class RenderViewHost;
class RenderWidgetHostViewGtk;

class ExtensionViewGtk {
 public:
  ExtensionViewGtk(ExtensionHost* extension_host, Browser* browser);

  void Init();

  gfx::NativeView native_view();
  Browser* browser() const { return browser_; }

  bool is_toolstrip() const { return is_toolstrip_; }
  void set_is_toolstrip(bool is_toolstrip) { is_toolstrip_ = is_toolstrip; }

  // Method for the ExtensionHost to notify us about the correct width for
  // extension contents.
  void UpdatePreferredWidth(int pref_width);

 private:
  RenderViewHost* render_view_host() const;

  void CreateWidgetHostView();

  // True if the contents are being displayed inside the extension shelf.
  bool is_toolstrip_;

  Browser* browser_;

  ExtensionHost* extension_host_;

  RenderWidgetHostViewGtk* render_widget_host_view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewGtk);
};

#endif  // CHROME_BROWSER_GTK_EXTENSION_VIEW_GTK_H_
