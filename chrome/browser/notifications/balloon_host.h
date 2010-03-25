// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/common/renderer_preferences.h"
#include "webkit/glue/webpreferences.h"

class Profile;

class BalloonHost : public RenderViewHostDelegate,
                    public RenderViewHostDelegate::View {
 public:
  explicit BalloonHost(Balloon* balloon);

  // Initialize the view.
  void Init();

  // Stops showing the balloon.
  void Shutdown();

  RenderViewHost* render_view_host() const { return render_view_host_; }

  // RenderViewHostDelegate overrides.
  virtual WebPreferences GetWebkitPrefs();
  virtual SiteInstance* GetSiteInstance() const {
    return site_instance_.get();
  }
  virtual Profile* GetProfile() const { return balloon_->profile(); }
  virtual const GURL& GetURL() const {
    return balloon_->notification().content_url();
  }
  virtual void Close(RenderViewHost* render_view_host);
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RendererReady(RenderViewHost* render_view_host);
  virtual void RendererGone(RenderViewHost* render_view_host);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id, const std::wstring& title) {}
  virtual int GetBrowserWindowID() const { return -1; }
  virtual ViewType::Type GetRenderViewType() const {
    return ViewType::TAB_CONTENTS;
  }
  virtual RenderViewHostDelegate::View* GetViewDelegate() {
    return this;
  }

  // RenderViewHostDelegate::View methods. Only the ones for opening new
  // windows are currently implemented.
  virtual void CreateNewWindow(int route_id);
  virtual void CreateNewWidget(int route_id, bool activatable) {}
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops) {}
  virtual void StartDragging(const WebDropData&,
                             WebKit::WebDragOperationsMask,
                             const SkBitmap&,
                             const gfx::Point&) {}
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}
  virtual void GotFocus() {}
  virtual void TakeFocus(bool reverse) {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) {
    return false;
  }
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void HandleMouseEvent() {}
  virtual void HandleMouseLeave() {}
  virtual void UpdatePreferredSize(const gfx::Size& pref_size);
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const {
    return RendererPreferences();
  }

 protected:
  // Must override in platform specific implementations.
  virtual void InitRenderWidgetHostView() = 0;
  virtual RenderWidgetHostView* render_widget_host_view() const = 0;

  // Owned pointer to the host for the renderer process.
  RenderViewHost* render_view_host_;

 private:
  // Non-owned pointer to the associated balloon.
  Balloon* balloon_;

  // True after Init() has completed.
  bool initialized_;

  // Indicates whether we should notify about disconnection of this balloon.
  // This is used to ensure disconnection notifications only happen if
  // a connection notification has happened and that they happen only once.
  bool should_notify_on_disconnect_;

  // Whether the page we are rendering is from an extension.
  bool is_extension_page_;

  // Site instance for the balloon/profile, to be used for opening new links.
  scoped_refptr<SiteInstance> site_instance_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
