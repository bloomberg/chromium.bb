// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
#pragma once

#include <vector>

#include "base/scoped_ptr.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/gfx/size.h"
#include "views/widget/widget_gtk.h"

class ConstrainedWindowGtk;
typedef struct _GtkFloatingContainer GtkFloatingContainer;
class RenderViewContextMenuViews;
class SadTabView;
class SkBitmap;
class TabContentsDragSource;
class WebDragDestGtk;
namespace gfx {
class Point;
}
namespace views {
class NativeViewHost;
}

// Gtk-specific implementation of the TabContentsView for the views-based front
// end. It is a WidgetGtk that contains all of the contents of the tab and
// associated child views.
class TabContentsViewGtk : public TabContentsView,
                           public views::WidgetGtk {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewGtk(TabContents* tab_contents);
  virtual ~TabContentsViewGtk();

  // Unlike Windows, ConstrainedWindows need to collaborate with the
  // TabContentsViewGtk to position the dialogs.
  void AttachConstrainedWindow(ConstrainedWindowGtk* constrained_window);
  void RemoveConstrainedWindow(ConstrainedWindowGtk* constrained_window);

  gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event);

  // TabContentsView implementation --------------------------------------------

  virtual void CreateView(const gfx::Size& initial_size);
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host);
  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView() const;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const;
  virtual void GetContainerBounds(gfx::Rect* out) const;
  virtual void SetPageTitle(const std::wstring& title);
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code);
  virtual void SizeContents(const gfx::Size& size);
  virtual void Focus();
  virtual void SetInitialFocus();
  virtual void StoreFocus();
  virtual void RestoreFocus();
  virtual void GetViewBounds(gfx::Rect* out) const;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned);
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask ops_allowed,
                             const SkBitmap& image,
                             const gfx::Point& image_offset);
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);

 private:
  // Signal handlers -----------------------------------------------------------

  // Overridden from views::WidgetGtk:
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  virtual gboolean OnPaint(GtkWidget* widget, GdkEventExpose* event);
  virtual void OnShow(GtkWidget* widget);
  virtual void OnHide(GtkWidget* widget);

  // Handles notifying the TabContents and other operations when the window was
  // shown or hidden.
  void WasHidden();
  void WasShown();

  // Handles resizing of the contents. This will notify the RenderWidgetHostView
  // of the change, reposition popups, and the find in page bar.
  void WasSized(const gfx::Size& size);

  // For any floating views (ConstrainedDialogs) this function centers them
  // within this view. It's called whem a ConstrainedDialog is attached and
  // when this view is resized.
  void SetFloatingPosition(const gfx::Size& size);

  // ---------------------------------------------------------------------------

  // Used to render the sad tab. This will be non-NULL only when the sad tab is
  // visible.
  SadTabView* sad_tab_;

  // Whether to ignore the next CHAR keyboard event.
  bool ignore_next_char_event_;

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuViews> context_menu_;

  // Handles drags from this TabContentsView.
  scoped_ptr<TabContentsDragSource> drag_source_;

  // The event for the last mouse down we handled. We need this for drags.
  GdkEventButton last_mouse_down_;

  // The helper object that handles drag destination related interactions with
  // GTK.
  scoped_ptr<WebDragDestGtk> drag_dest_;

  // Current size. See comment in WidgetGtk as to why this is cached.
  gfx::Size size_;

  // Each individual UI for constrained dialogs currently displayed. The
  // objects in this vector are owned by the TabContents, not the view.
  std::vector<ConstrainedWindowGtk*> constrained_windows_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewGtk);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
