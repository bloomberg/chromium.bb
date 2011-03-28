// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_MAC_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/base_view.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/size.h"

@class FocusTracker;
@class SadTabController;
class SkBitmap;
class TabContentsViewMac;
@class WebDragSource;
@class WebDropTarget;
namespace gfx {
class Point;
}

@interface TabContentsViewCocoa : BaseView {
 @private
  TabContentsViewMac* tabContentsView_;  // WEAK; owns us
  scoped_nsobject<WebDragSource> dragSource_;
  scoped_nsobject<WebDropTarget> dropTarget_;
}

// Expose this, since sometimes one needs both the NSView and the TabContents.
- (TabContents*)tabContents;
@end

// Mac-specific implementation of the TabContentsView. It owns an NSView that
// contains all of the contents of the tab and associated child views.
class TabContentsViewMac : public TabContentsView,
                           public NotificationObserver {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewMac(TabContents* web_contents);
  virtual ~TabContentsViewMac();

  // TabContentsView implementation --------------------------------------------

  virtual void CreateView(const gfx::Size& initial_size);
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host);
  virtual gfx::NativeView GetNativeView() const;
  virtual gfx::NativeView GetContentNativeView() const;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const;
  virtual void GetContainerBounds(gfx::Rect* out) const;
  virtual void RenderViewCreated(RenderViewHost* host);
  virtual void SetPageTitle(const std::wstring& title);
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code);
  virtual void SizeContents(const gfx::Size& size);
  virtual void Focus();
  virtual void SetInitialFocus();
  virtual void StoreFocus();
  virtual void RestoreFocus();
  virtual void UpdatePreferredSize(const gfx::Size& pref_size);
  virtual RenderWidgetHostView* CreateNewWidgetInternal(
      int route_id,
      WebKit::WebPopupType popup_type);
  virtual void ShowCreatedWidgetInternal(RenderWidgetHostView* widget_host_view,
                                         const gfx::Rect& initial_pos);
  virtual bool IsEventTracking() const;
  virtual void CloseTabAfterEventTracking();
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
                             WebKit::WebDragOperationsMask allowed_operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset);
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);

  // NotificationObserver implementation ---------------------------------------

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // A helper method for closing the tab in the
  // CloseTabAfterEventTracking() implementation.
  void CloseTab();

  int preferred_width() const { return preferred_width_; }

 private:
  // The Cocoa NSView that lives in the view hierarchy.
  scoped_nsobject<TabContentsViewCocoa> cocoa_view_;

  // Keeps track of which NSView has focus so we can restore the focus when
  // focus returns.
  scoped_nsobject<FocusTracker> focus_tracker_;

  // Used to get notifications about renderers coming and going.
  NotificationRegistrar registrar_;

  // Used to render the sad tab. This will be non-NULL only when the sad tab is
  // visible.
  scoped_nsobject<SadTabController> sad_tab_;

  // The page content's intrinsic width.
  int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewMac);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_MAC_H_
