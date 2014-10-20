// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"

namespace content {

class WebContents;
class WebContentsImpl;
class BrowserPluginGuest;

class WebContentsViewGuest : public WebContentsView,
                             public RenderViewHostDelegateView {
 public:
  // The corresponding WebContentsImpl is passed in the constructor, and manages
  // our lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  // WebContentsViewGuest always has a backing platform dependent view,
  // |platform_view|.
  WebContentsViewGuest(WebContentsImpl* web_contents,
                       BrowserPluginGuest* guest,
                       scoped_ptr<WebContentsView> platform_view,
                       RenderViewHostDelegateView* platform_view_delegate_view);
  virtual ~WebContentsViewGuest();

  WebContents* web_contents();

  void OnGuestAttached(WebContentsView* parent_view);

  void OnGuestDetached(WebContentsView* old_parent_view);

  // Converts the guest specific coordinates in |params| to embedder specific
  // ones.
  ContextMenuParams ConvertContextMenuParams(
      const ContextMenuParams& params) const;

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
  virtual void CreateView(const gfx::Size& initial_size,
                          gfx::NativeView context) override;
  virtual RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host, bool is_guest_view_hack) override;
  virtual RenderWidgetHostViewBase* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) override;
  virtual void SetPageTitle(const base::string16& title) override;
  virtual void RenderViewCreated(RenderViewHost* host) override;
  virtual void RenderViewSwappedIn(RenderViewHost* host) override;
  virtual void SetOverscrollControllerEnabled(bool enabled) override;
#if defined(OS_MACOSX)
  virtual void SetAllowOtherViews(bool allow) override;
  virtual bool GetAllowOtherViews() const override;
  virtual bool IsEventTracking() const override;
  virtual void CloseTabAfterEventTracking() override;
#endif

  // Backend implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) override;
  virtual void StartDragging(const DropData& drop_data,
                             blink::WebDragOperationsMask allowed_ops,
                             const gfx::ImageSkia& image,
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) override;
  virtual void UpdateDragCursor(blink::WebDragOperation operation) override;
  virtual void GotFocus() override;
  virtual void TakeFocus(bool reverse) override;

 private:
  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;
  BrowserPluginGuest* guest_;
  // The platform dependent view backing this WebContentsView.
  // Calls to this WebContentsViewGuest are forwarded to |platform_view_|.
  scoped_ptr<WebContentsView> platform_view_;
  gfx::Size size_;

  // Delegate view for guest's platform view.
  RenderViewHostDelegateView* platform_view_delegate_view_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_GUEST_H_
