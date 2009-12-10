// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_NOTIFICATIONS_BALLOON_VIEW_HOST_H_
#define CHROME_BROWSER_VIEWS_NOTIFICATIONS_BALLOON_VIEW_HOST_H_

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/common/renderer_preferences.h"
#include "views/controls/native/native_view_host.h"
#include "webkit/glue/webpreferences.h"

class Profile;
class RenderViewHost;

// BalloonViewHost class is a delegate to the renderer host for the HTML
// notification.  When initialized it creates a new RenderViewHost and loads
// the contents of the toast into it.  It also handles links within the toast,
// loading them into a new tab.
class BalloonViewHost : public views::NativeViewHost,
                        public RenderViewHostDelegate,
                        public RenderViewHostDelegate::View {
 public:
  explicit BalloonViewHost(Balloon* balloon);

  ~BalloonViewHost() {
     Shutdown();
  }

  // Stops showing the balloon.
  void Shutdown();

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
  virtual void UpdateTitle(RenderViewHost* /* render_view_host */,
                           int32 /* page_id */, const std::wstring& title) {
    title_ = title;
  }
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
                                 bool user_gesture,
                                 const GURL& creator_url);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops) {}
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
  virtual RendererPreferences GetRendererPrefs() const {
    return RendererPreferences();
  }

  // Accessors.
  RenderViewHost* render_view_host() const { return render_view_host_; }
  const std::wstring& title() const { return title_; }

 private:
  // View overrides.
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View *parent,
                                    views::View *child);

  // Initialize the view, parented to |parent|, and show it.
  void Init(gfx::NativeView parent);

  // True after Init() has completed.
  bool initialized_;

  // Non-owned pointer to the associated balloon.
  Balloon* balloon_;

  // Site instance for the balloon/profile, to be used for opening new links.
  scoped_refptr<SiteInstance> site_instance_;

  // Owned pointer to to host for the renderer process.
  RenderViewHost* render_view_host_;

  // Indicates whether we should notify about disconnection of this balloon.
  // This is used to ensure disconnection notifications only happen if
  // a connection notification has happened and that they happen only once.
  bool should_notify_on_disconnect_;

  // The title of the balloon page.
  std::wstring title_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewHost);
};

#endif  // CHROME_BROWSER_VIEWS_NOTIFICATIONS_BALLOON_VIEW_HOST_H_
