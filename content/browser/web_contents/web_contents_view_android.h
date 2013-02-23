// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/web_contents_view_port.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"

namespace content {
class ContentViewCoreImpl;

// Android-specific implementation of the WebContentsView.
class WebContentsViewAndroid : public WebContentsViewPort,
                               public RenderViewHostDelegateView {
 public:
  WebContentsViewAndroid(WebContentsImpl* web_contents,
                         WebContentsViewDelegate* delegate);
  virtual ~WebContentsViewAndroid();

  // Sets the interface to the view system. ContentViewCoreImpl is owned
  // by its Java ContentViewCore counterpart, whose lifetime is managed
  // by the UI frontend.
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);

  // WebContentsView implementation --------------------------------------------

  virtual void CreateView(
      const gfx::Size& initial_size, gfx::NativeView context) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
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

  // WebContentsViewPort implementation ----------------------------------------
  virtual void RenderViewSwappedIn(RenderViewHost* host) OVERRIDE;

  // Backend implementation of RenderViewHostDelegateView.
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

 private:
  // The WebContents whose contents we display.
  WebContentsImpl* web_contents_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  // Interface for extensions to WebContentsView. Used to show the context menu.
  scoped_ptr<WebContentsViewDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
