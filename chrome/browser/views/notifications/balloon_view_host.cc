// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/notifications/balloon_view_host.h"

#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#if defined(OS_WIN)
#include "chrome/browser/renderer_host/render_widget_host_view_win.h"
#endif
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/renderer_preferences.h"
#include "views/widget/widget.h"
#include "views/widget/widget_win.h"

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : balloon_(balloon),
      site_instance_(SiteInstance::CreateSiteInstance(balloon->profile())),
      render_view_host_(NULL),
      should_notify_on_disconnect_(false),
      initialized_(false) {
  DCHECK(balloon_);
}

void BalloonViewHost::Shutdown() {
  if (render_view_host_) {
    render_view_host_->Shutdown();
    render_view_host_ = NULL;
  }
}

RendererPreferences BalloonViewHost::GetRendererPrefs() const {
  // We want links (a.k.a. top_level_requests) to be forwarded to the browser so
  // that we can open them in a new tab rather than in the balloon.
  RendererPreferences prefs = RendererPreferences();
  prefs.browser_handles_top_level_requests = true;
  return prefs;
}

void BalloonViewHost::RequestOpenURL(const GURL& url,
                                     const GURL& referrer,
                                     WindowOpenDisposition disposition) {
  // Always open a link triggered within the notification balloon in a new tab.
  BrowserList::GetLastActive()->AddTabWithURL(url, referrer,
      PageTransition::LINK, true, 0, 0, GetSiteInstance());
}

void BalloonViewHost::RendererReady(RenderViewHost* /* render_view_host */) {
  should_notify_on_disconnect_ = true;
  NotificationService::current()->Notify(
      NotificationType::NOTIFY_BALLOON_CONNECTED,
      Source<Balloon>(balloon_), NotificationService::NoDetails());
}

void BalloonViewHost::RendererGone(RenderViewHost* /* render_view_host */) {
  if (!should_notify_on_disconnect_)
    return;

  should_notify_on_disconnect_ = false;
  NotificationService::current()->Notify(
      NotificationType::NOTIFY_BALLOON_DISCONNECTED,
      Source<Balloon>(balloon_), NotificationService::NoDetails());
}

void BalloonViewHost::Init(gfx::NativeView parent_hwnd) {
  DCHECK(!render_view_host_) << "BalloonViewHost already initialized.";
  RenderViewHost* rvh = new RenderViewHost(site_instance_.get(),
                                           this, MSG_ROUTING_NONE);
  render_view_host_ = rvh;

  // Pointer is owned by the RVH.
  RenderWidgetHostView* view = RenderWidgetHostView::CreateViewForWidget(rvh);
  rvh->set_view(view);

  // TODO(johnnyg): http://crbug.com/23954.  Need a cross-platform solution.
#if defined(OS_WIN)
  RenderWidgetHostViewWin* view_win =
      static_cast<RenderWidgetHostViewWin*>(view);

  // Create the HWND.
  HWND hwnd = view_win->Create(parent_hwnd);
  view_win->ShowWindow(SW_SHOW);
  Attach(hwnd);
#else
  NOTIMPLEMENTED();
#endif

  // Start up the renderer and point it at the balloon contents URL.
  rvh->CreateRenderView();
  rvh->NavigateToURL(balloon_->notification().content_url());
  initialized_ = true;
}

void BalloonViewHost::ViewHierarchyChanged(bool is_add, views::View* parent,
                                           views::View* child) {
  NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !initialized_)
    Init(GetWidget()->GetNativeView());
}
