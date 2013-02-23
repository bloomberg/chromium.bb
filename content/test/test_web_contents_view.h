// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_WEB_CONTENTS_VIEW_H_
#define CONTENT_TEST_TEST_WEB_CONTENTS_VIEW_H_

#include "base/compiler_specific.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/web_contents_view_port.h"

namespace content {

class TestWebContentsView : public WebContentsViewPort,
                            public RenderViewHostDelegateView {
 public:
  TestWebContentsView();
  virtual ~TestWebContentsView();

  // RenderViewHostDelegateView:
  virtual void ShowContextMenu(const ContextMenuParams& params,
                               ContextMenuSourceType type) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

  // WebContentsView:
  virtual void CreateView(const gfx::Size& initial_size,
                          gfx::NativeView context) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect *out) const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual WebDropData* GetDropData() const OVERRIDE;
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
#if defined(OS_MACOSX)
  virtual void SetAllowOverlappingViews(bool overlapping) OVERRIDE;
#endif

  // WebContentsViewPort:
  virtual void RenderViewSwappedIn(RenderViewHost* host) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebContentsView);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_WEB_CONTENTS_VIEW_H_
