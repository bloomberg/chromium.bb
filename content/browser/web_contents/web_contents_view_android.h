// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"
#include "ui/gfx/rect_f.h"

namespace content {
class ContentViewCoreImpl;
class WebContentsImpl;

// Android-specific implementation of the WebContentsView.
class WebContentsViewAndroid : public WebContentsView,
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
  virtual gfx::NativeView GetNativeView() const override;
  virtual gfx::NativeView GetContentNativeView() const override;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const override;
  virtual void GetContainerBounds(gfx::Rect* out) const override;
  virtual void SizeContents(const gfx::Size& size) override;
  virtual void Focus() override;
  virtual void SetInitialFocus() override;
  virtual void StoreFocus() override;
  virtual void RestoreFocus() override;
  virtual DropData* GetDropData() const override;
  virtual gfx::Rect GetViewBounds() const override;
  virtual void CreateView(
      const gfx::Size& initial_size, gfx::NativeView context) override;
  virtual RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host, bool is_guest_view_hack) override;
  virtual RenderWidgetHostViewBase* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) override;
  virtual void SetPageTitle(const base::string16& title) override;
  virtual void RenderViewCreated(RenderViewHost* host) override;
  virtual void RenderViewSwappedIn(RenderViewHost* host) override;
  virtual void SetOverscrollControllerEnabled(bool enabled) override;

  // Backend implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) override;
  virtual void ShowPopupMenu(RenderFrameHost* render_frame_host,
                             const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<MenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) override;
  virtual void HidePopupMenu() override;
  virtual void StartDragging(const DropData& drop_data,
                             blink::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) override;
  virtual void UpdateDragCursor(blink::WebDragOperation operation) override;
  virtual void GotFocus() override;
  virtual void TakeFocus(bool reverse) override;

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
