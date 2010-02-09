// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/notifications/balloon_view_host_mac.h"

#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : initialized_(false),
      balloon_(balloon),
      site_instance_(SiteInstance::CreateSiteInstance(balloon->profile())),
      render_view_host_(NULL),
      should_notify_on_disconnect_(false),
      render_widget_host_view_(NULL) {
  DCHECK(balloon_);
}

void BalloonViewHost::Shutdown() {
  if (render_view_host_) {
    render_view_host_->Shutdown();
    render_view_host_ = NULL;
  }
}

WebPreferences BalloonViewHost::GetWebkitPrefs() {
  WebPreferences prefs;
  prefs.allow_scripts_to_close_windows = true;
  return prefs;
}

RendererPreferences BalloonViewHost::GetRendererPrefs(Profile* profile) const {
  RendererPreferences prefs;
  renderer_preferences_util::UpdateFromSystemSettings(&prefs, profile);
  return prefs;
}

void BalloonViewHost::Close(RenderViewHost* render_view_host) {
  balloon_->CloseByScript();
}

void BalloonViewHost::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(new ViewMsg_EnablePreferredSizeChangedMode(
      render_view_host->routing_id()));
}

void BalloonViewHost::RendererReady(RenderViewHost* render_view_host) {
  should_notify_on_disconnect_ = true;
  NotificationService::current()->Notify(
      NotificationType::NOTIFY_BALLOON_CONNECTED,
      Source<Balloon>(balloon_), NotificationService::NoDetails());
}

void BalloonViewHost::RendererGone(RenderViewHost* render_view_host) {
  if (!should_notify_on_disconnect_)
    return;

  should_notify_on_disconnect_ = false;
  NotificationService::current()->Notify(
      NotificationType::NOTIFY_BALLOON_DISCONNECTED,
      Source<Balloon>(balloon_), NotificationService::NoDetails());
}

// RenderViewHostDelegate::View methods implemented to allow links to
// open pages in new tabs.
void BalloonViewHost::CreateNewWindow(int route_id) {
  delegate_view_helper_.CreateNewWindow(
      route_id, balloon_->profile(), site_instance_.get(),
      DOMUIFactory::GetDOMUIType(balloon_->notification().content_url()), NULL);
}

void BalloonViewHost::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture) {
  // Don't allow pop-ups from notifications.
  if (disposition == NEW_POPUP)
    return;

  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    Browser* browser = BrowserList::GetLastActive();
    browser->AddTabContents(contents, disposition, initial_pos, user_gesture);
  }
}

void BalloonViewHost::UpdatePreferredSize(const gfx::Size& new_size) {
  balloon_->SetContentPreferredSize(new_size);
}

void BalloonViewHost::UpdateActualSize(const gfx::Size& new_size) {
  NSView* view = render_widget_host_view_->native_view();
  NSRect frame = [view frame];
  frame.size.width = new_size.width();
  frame.size.height = new_size.height();

  [view setFrame:frame];
  [view setNeedsDisplay:YES];
}

void BalloonViewHost::Init() {
  DCHECK(!render_view_host_) << "BalloonViewHost already initialized.";

  int64 session_storage_namespace_id = balloon_->profile()->GetWebKitContext()->
      dom_storage_context()->AllocateSessionStorageNamespaceId();

  render_view_host_ = new RenderViewHost(site_instance_.get(),
                                         this, MSG_ROUTING_NONE,
                                         session_storage_namespace_id);

  render_widget_host_view_ = new RenderWidgetHostViewMac(render_view_host_);
  render_view_host_->set_view(render_widget_host_view_);
  render_view_host_->CreateRenderView(GetProfile()->GetRequestContext());
  render_view_host_->NavigateToURL(balloon_->notification().content_url());
  initialized_ = true;
}
