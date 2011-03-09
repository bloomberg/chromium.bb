// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_H_
#pragma once

#include "build/build_config.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/native/native_view_host.h"

class Browser;
class Extension;
class ExtensionHost;
class ExtensionView;
class RenderViewHost;

// This handles the display portion of an ExtensionHost.
class ExtensionView : public views::NativeViewHost {
 public:
  ExtensionView(ExtensionHost* host, Browser* browser);
  ~ExtensionView();

  // A class that represents the container that this view is in.
  // (bottom shelf, side bar, etc.)
  class Container {
   public:
    virtual ~Container() {}
    // Mouse event notifications from the view. (useful for hover UI).
    virtual void OnExtensionMouseMove(ExtensionView* view) = 0;
    virtual void OnExtensionMouseLeave(ExtensionView* view) = 0;
    virtual void OnExtensionPreferredSizeChanged(ExtensionView* view) {}
  };

  ExtensionHost* host() const { return host_; }
  Browser* browser() const { return browser_; }
  const Extension* extension() const;
  RenderViewHost* render_view_host() const;
  void DidStopLoading();
  void SetIsClipped(bool is_clipped);

  // Notification from ExtensionHost.
  void UpdatePreferredSize(const gfx::Size& new_size);
  void HandleMouseMove();
  void HandleMouseLeave();

  // Method for the ExtensionHost to notify us when the RenderViewHost has a
  // connection.
  void RenderViewCreated();

  // Set a custom background for the view. The background will be tiled.
  void SetBackground(const SkBitmap& background);

  // Sets the container for this view.
  void SetContainer(Container* container) { container_ = container; }

  // Overridden from views::NativeViewHost:
  virtual void SetVisible(bool is_visible) OVERRIDE;
  virtual void ViewHierarchyChanged(
      bool is_add, views::View *parent, views::View *child) OVERRIDE;

 protected:
  // Overridden from views::View.
  virtual void PreferredSizeChanged() OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  friend class ExtensionHost;

  // Initializes the RenderWidgetHostView for this object.
  void CreateWidgetHostView();

  // We wait to show the ExtensionView until several things have loaded.
  void ShowIfCompletelyLoaded();

  // Restore object to initial state. Called on shutdown or after a renderer
  // crash.
  void CleanUp();

  // The running extension instance that we're displaying.
  // Note that host_ owns view
  ExtensionHost* host_;

  // The browser window that this view is in.
  Browser* browser_;

  // True if we've been initialized.
  bool initialized_;

  // The background the view should have once it is initialized. This is set
  // when the view has a custom background, but hasn't been initialized yet.
  SkBitmap pending_background_;

  // What we should set the preferred width to once the ExtensionView has
  // loaded.
  gfx::Size pending_preferred_size_;

  // The container this view is in (not necessarily its direct superview).
  // Note: the view does not own its container.
  Container* container_;

  // Whether this extension view is clipped.
  bool is_clipped_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_H_
