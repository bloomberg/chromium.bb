// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_GTK_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_GTK_H_
#pragma once

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view.h"
#include "ui/views/widget/native_widget_gtk.h"

class ConstrainedWindowGtk;
class TabContents;
class WebDragBookmarkHandlerGtk;

namespace content {
class WebDragDestGtk;
class WebDragSourceGtk;
}

class NativeTabContentsViewGtk : public views::NativeWidgetGtk,
                                 public NativeTabContentsView {
 public:
  explicit NativeTabContentsViewGtk(
      internal::NativeTabContentsViewDelegate* delegate);
  virtual ~NativeTabContentsViewGtk();

  // Unlike Windows, ConstrainedWindows need to collaborate with the
  // TabContentsViewGtk to position the dialogs.
  void AttachConstrainedWindow(ConstrainedWindowGtk* constrained_window);
  void RemoveConstrainedWindow(ConstrainedWindowGtk* constrained_window);


 private:
  // Overridden from NativeTabContentsView:
  virtual void InitNativeTabContentsView() OVERRIDE;
  virtual void Unparent() OVERRIDE;
  virtual RenderWidgetHostView* CreateRenderWidgetHostView(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void CancelDrag() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void SetDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;

  // Overridden from views::NativeWidgetGtk:
  virtual gboolean OnMotionNotify(GtkWidget* widget,
                                  GdkEventMotion* event) OVERRIDE;
  virtual gboolean OnLeaveNotify(GtkWidget* widget,
                                 GdkEventCrossing* event) OVERRIDE;
  virtual gboolean OnButtonPress(GtkWidget* widget,
                                 GdkEventButton* event) OVERRIDE;
  virtual void OnSizeAllocate(GtkWidget* widget,
                              GtkAllocation* allocation) OVERRIDE;
  virtual void OnShow(GtkWidget* widget) OVERRIDE;
  virtual void OnHide(GtkWidget* widget) OVERRIDE;

  void PositionConstrainedWindows(const gfx::Size& view_size);

  internal::NativeTabContentsViewDelegate* delegate_;

  // Whether to ignore the next CHAR keyboard event.
  bool ignore_next_char_event_;

  // Handles drags from this TabContentsView.
  scoped_ptr<content::WebDragSourceGtk> drag_source_;

  // The event for the last mouse down we handled. We need this for drags.
  GdkEventButton last_mouse_down_;

  // The helper object that handles drag destination related interactions with
  // GTK.
  scoped_ptr<content::WebDragDestGtk> drag_dest_;

  // The chrome specific delegate that receives events from WebDragDestGtk.
  scoped_ptr<WebDragBookmarkHandlerGtk> bookmark_handler_gtk_;

  // Current size. See comment in NativeWidgetGtk as to why this is cached.
  gfx::Size size_;

  // Each individual UI for constrained dialogs currently displayed. The
  // objects in this vector are owned by the TabContents, not the view.
  std::vector<ConstrainedWindowGtk*> constrained_windows_;

  DISALLOW_COPY_AND_ASSIGN(NativeTabContentsViewGtk);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_GTK_H_

