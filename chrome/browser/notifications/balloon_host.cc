// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon_host.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/url_constants.h"

BalloonHost::BalloonHost(Balloon* balloon)
    : render_view_host_(NULL),
      balloon_(balloon),
      initialized_(false),
      should_notify_on_disconnect_(false),
      is_extension_page_(false) {
  DCHECK(balloon_);

  // If the notification is for an extension URL, make sure to use the extension
  // process to render it, so that it can communicate with other views in the
  // extension.
  const GURL& balloon_url = balloon_->notification().content_url();
  if (balloon_url.SchemeIs(chrome::kExtensionScheme)) {
    is_extension_page_ = true;
    site_instance_ =
      balloon_->profile()->GetExtensionProcessManager()->GetSiteInstanceForURL(
          balloon_url);
  } else {
    site_instance_ = SiteInstance::CreateSiteInstance(balloon_->profile());
  }
}

void BalloonHost::Shutdown() {
  if (render_view_host_) {
    render_view_host_->Shutdown();
    render_view_host_ = NULL;
  }
}

WebPreferences BalloonHost::GetWebkitPrefs() {
  WebPreferences prefs;
  prefs.allow_scripts_to_close_windows = true;
  return prefs;
}

void BalloonHost::Close(RenderViewHost* render_view_host) {
  balloon_->CloseByScript();
}


void BalloonHost::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(new ViewMsg_EnablePreferredSizeChangedMode(
      render_view_host->routing_id()));
}

void BalloonHost::RendererReady(RenderViewHost* render_view_host) {
  should_notify_on_disconnect_ = true;
  NotificationService::current()->Notify(
      NotificationType::NOTIFY_BALLOON_CONNECTED,
      Source<Balloon>(balloon_), NotificationService::NoDetails());
}

void BalloonHost::RendererGone(RenderViewHost* render_view_host) {
  if (!should_notify_on_disconnect_)
    return;

  should_notify_on_disconnect_ = false;
  NotificationService::current()->Notify(
      NotificationType::NOTIFY_BALLOON_DISCONNECTED,
      Source<Balloon>(balloon_), NotificationService::NoDetails());
}

// RenderViewHostDelegate::View methods implemented to allow links to
// open pages in new tabs.
void BalloonHost::CreateNewWindow(int route_id) {
  delegate_view_helper_.CreateNewWindow(
      route_id, balloon_->profile(), site_instance_.get(),
      DOMUIFactory::GetDOMUIType(balloon_->notification().content_url()), NULL);
}

void BalloonHost::ShowCreatedWindow(int route_id,
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

void BalloonHost::UpdatePreferredSize(const gfx::Size& new_size) {
  balloon_->SetContentPreferredSize(new_size);
}

void BalloonHost::Init() {
  DCHECK(!render_view_host_) << "BalloonViewHost already initialized.";
  int64 session_storage_namespace_id = balloon_->profile()->GetWebKitContext()->
      dom_storage_context()->AllocateSessionStorageNamespaceId();
  RenderViewHost* rvh = new RenderViewHost(site_instance_.get(),
                                           this, MSG_ROUTING_NONE,
                                           session_storage_namespace_id);
  if (is_extension_page_)
    rvh->AllowBindings(BindingsPolicy::EXTENSION);

  // Do platform-specific initialization.
  render_view_host_ = rvh;
  InitRenderWidgetHostView();
  DCHECK(render_widget_host_view());

  rvh->set_view(render_widget_host_view());
  rvh->CreateRenderView(GetProfile()->GetRequestContext());
  rvh->NavigateToURL(balloon_->notification().content_url());

  initialized_ = true;
}
