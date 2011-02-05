// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSION_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSION_VIEW_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class Browser;
class ExtensionHost;
class RenderViewHost;
class RenderWidgetHostViewGtk;
class SkBitmap;

class ExtensionViewGtk {
 public:
  ExtensionViewGtk(ExtensionHost* extension_host, Browser* browser);

  class Container {
   public:
    virtual ~Container() {}
    virtual void OnExtensionPreferredSizeChanged(ExtensionViewGtk* view,
                                                 const gfx::Size& new_size) {}
  };

  void Init();

  gfx::NativeView native_view();
  Browser* browser() const { return browser_; }

  void SetBackground(const SkBitmap& background);

  // Sets the container for this view.
  void SetContainer(Container* container) { container_ = container; }

  // Method for the ExtensionHost to notify us about the correct size for
  // extension contents.
  void UpdatePreferredSize(const gfx::Size& new_size);

  // Method for the ExtensionHost to notify us when the RenderViewHost has a
  // connection.
  void RenderViewCreated();

  RenderViewHost* render_view_host() const;

 private:
  void CreateWidgetHostView();

  Browser* browser_;

  ExtensionHost* extension_host_;

  RenderWidgetHostViewGtk* render_widget_host_view_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // This view's container.
  Container* container_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSION_VIEW_GTK_H_
