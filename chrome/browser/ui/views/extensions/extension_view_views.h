// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

class Browser;

namespace content {
class RenderViewHost;
}

namespace extensions {
class Extension;
class ExtensionHost;
}

// This handles the display portion of an ExtensionHost.
class ExtensionViewViews : public views::NativeViewHost {
 public:
  ExtensionViewViews(extensions::ExtensionHost* host, Browser* browser);
  virtual ~ExtensionViewViews();

  // A class that represents the container that this view is in.
  // (bottom shelf, side bar, etc.)
  class Container {
   public:
    virtual ~Container() {}
    virtual void OnExtensionSizeChanged(ExtensionViewViews* view) {}
    virtual void OnViewWasResized() {}
  };

  extensions::ExtensionHost* host() const { return host_; }
  Browser* browser() const { return browser_; }
  const extensions::Extension* extension() const;
  content::RenderViewHost* render_view_host() const;
  void DidStopLoading();
  void SetIsClipped(bool is_clipped);

  // Notification from ExtensionHost.
  void ResizeDueToAutoResize(const gfx::Size& new_size);

  // Method for the ExtensionHost to notify us when the RenderViewHost has a
  // connection.
  void RenderViewCreated();

  // Sets the container for this view.
  void SetContainer(Container* container) { container_ = container; }

  // Handles unhandled keyboard messages coming back from the renderer process.
  void HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event);

  // Overridden from views::NativeViewHost:
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;
  virtual void SetVisible(bool is_visible) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

 private:
  // Overridden from views::View.
  virtual bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void PreferredSizeChanged() OVERRIDE;
  virtual void OnFocus() OVERRIDE;

 private:
  friend class extensions::ExtensionHost;

  // Initializes the RenderWidgetHostView for this object.
  void CreateWidgetHostView();

  // We wait to show the ExtensionViewViews until several things have loaded.
  void ShowIfCompletelyLoaded();

  // Restore object to initial state. Called on shutdown or after a renderer
  // crash.
  void CleanUp();

  // The running extension instance that we're displaying.
  // Note that host_ owns view
  extensions::ExtensionHost* host_;

  // The browser window that this view is in.
  Browser* browser_;

  // True if we've been initialized.
  bool initialized_;

  // What we should set the preferred width to once the ExtensionViewViews has
  // loaded.
  gfx::Size pending_preferred_size_;

  // The container this view is in (not necessarily its direct superview).
  // Note: the view does not own its container.
  Container* container_;

  // Whether this extension view is clipped.
  bool is_clipped_;

  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_VIEWS_H_
