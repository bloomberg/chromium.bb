// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_MAC_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_MAC_H_
#pragma once

#if defined(__OBJC__)

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/base/cocoa/base_view.h"
#include "ui/gfx/size.h"

@class FocusTracker;
class RenderViewContextMenuMac;
@class SadTabController;
class SkBitmap;
class TabContentsViewMac;
class WebDragBookmarkHandlerMac;
@class WebDragDest;
@class WebDragSource;
namespace gfx {
class Point;
}

@interface TabContentsViewCocoa : BaseView {
 @private
  TabContentsViewMac* tabContentsView_;  // WEAK; owns us
  scoped_nsobject<WebDragSource> dragSource_;
  scoped_nsobject<WebDragDest> dragDest_;
  scoped_ptr<WebDragBookmarkHandlerMac> bookmarkHandler_;
}

// Expose this, since sometimes one needs both the NSView and the TabContents.
- (TabContents*)tabContents;

// Set frame, then notify the RenderWidgetHost that the frame has been changed,
// but do it in a separate task, using |performSelector:withObject:afterDelay:|.
// This stops the flickering issue in http://crbug.com/31970
- (void)setFrameWithDeferredUpdate:(NSRect)frame;
@end

// Mac-specific implementation of the TabContentsView. It owns an NSView that
// contains all of the contents of the tab and associated child views.
class TabContentsViewMac : public TabContentsView {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewMac(TabContents* web_contents);
  virtual ~TabContentsViewMac();

  // TabContentsView implementation --------------------------------------------

  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void CancelDragAndCloseTab() OVERRIDE;
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;
  virtual void InstallOverlayView(gfx::NativeView view) OVERRIDE;
  virtual void RemoveOverlayView() OVERRIDE;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) OVERRIDE;
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

  // A helper method for closing the tab in the
  // CloseTabAfterEventTracking() implementation.
  void CloseTab();

  TabContents* tab_contents() { return tab_contents_; }
  int preferred_width() const { return preferred_width_; }

 private:
  // The TabContents whose contents we display.
  TabContents* tab_contents_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // The Cocoa NSView that lives in the view hierarchy.
  scoped_nsobject<TabContentsViewCocoa> cocoa_view_;

  // Keeps track of which NSView has focus so we can restore the focus when
  // focus returns.
  scoped_nsobject<FocusTracker> focus_tracker_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuMac> context_menu_;

  // The page content's intrinsic width.
  int preferred_width_;

  // The overlaid view. Owned by the caller of |InstallOverlayView|; this is a
  // weak reference.
  NSView* overlaid_view_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewMac);
};

#endif  // __OBJC__

// Functions that may be accessed from non-Objective-C C/C++ code.
class TabContents;
class TabContentsView;

namespace tab_contents_view_mac {
TabContentsView* CreateTabContentsView(TabContents* tab_contents);
}

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_MAC_H_
