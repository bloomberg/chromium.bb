// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/background_contents.h"

#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "ui/gfx/rect.h"

////////////////
// BackgroundContents

BackgroundContents::BackgroundContents(SiteInstance* site_instance,
                                       int routing_id,
                                       Delegate* delegate)
    : delegate_(delegate) {
  Profile* profile = Profile::FromBrowserContext(
      site_instance->browsing_instance()->browser_context());

  // TODO(rafaelw): Implement correct session storage.
  render_view_host_ = new RenderViewHost(site_instance, this, routing_id, NULL);

  // Close ourselves when the application is shutting down.
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 NotificationService::AllSources());

  // Register for our parent profile to shutdown, so we can shut ourselves down
  // as well (should only be called for OTR profiles, as we should receive
  // APP_TERMINATING before non-OTR profiles are destroyed).
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 Source<Profile>(profile));
}

// Exposed to allow creating mocks.
BackgroundContents::BackgroundContents()
    : delegate_(NULL),
      render_view_host_(NULL) {
}

BackgroundContents::~BackgroundContents() {
  if (!render_view_host_)   // Will be null for unit tests.
    return;
  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->process()->browser_context());
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
      Source<Profile>(profile),
      Details<BackgroundContents>(this));
  render_view_host_->Shutdown();  // deletes render_view_host
}

BackgroundContents* BackgroundContents::GetAsBackgroundContents() {
  return this;
}

RenderViewHostDelegate::View* BackgroundContents::GetViewDelegate() {
  return this;
}

const GURL& BackgroundContents::GetURL() const {
  return url_;
}

ViewType::Type BackgroundContents::GetRenderViewType() const {
  return ViewType::BACKGROUND_CONTENTS;
}

void BackgroundContents::DidNavigate(
    RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We only care when the outer frame changes.
  if (!PageTransition::IsMainFrame(params.transition))
    return;

  // Note: because BackgroundContents are only available to extension apps,
  // navigation is limited to urls within the app's extent. This is enforced in
  // RenderView::decidePolicyForNaviation. If BackgroundContents become
  // available as a part of the web platform, it probably makes sense to have
  // some way to scope navigation of a background page to its opener's security
  // origin. Note: if the first navigation is to a URL outside the app's
  // extent a background page will be opened but will remain at about:blank.
  url_ = params.url;

  Profile* profile = Profile::FromBrowserContext(
      render_view_host->process()->browser_context());
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
      Source<Profile>(profile),
      Details<BackgroundContents>(this));
}

void BackgroundContents::RunJavaScriptMessage(
    const RenderViewHost* rvh,
    const string16& message,
    const string16& default_prompt,
    const GURL& frame_url,
    const int flags,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // TODO(rafaelw): Implement.

  // Since we are suppressing messages, just reply as if the user immediately
  // pressed "Cancel".
  OnDialogClosed(reply_msg, false, string16());

  *did_suppress_message = true;
}

void BackgroundContents::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  // TODO(rafaelw): Implement pagegroup ref-counting so that non-persistent
  // background pages are closed when the last referencing frame is closed.
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
    case content::NOTIFICATION_APP_TERMINATING: {
      delete this;
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void BackgroundContents::OnDialogClosed(IPC::Message* reply_msg,
                                        bool success,
                                        const string16& user_input) {
  render_view_host()->JavaScriptDialogClosed(reply_msg,
                                             success,
                                             user_input);
}

gfx::NativeWindow BackgroundContents::GetDialogRootWindow() {
  NOTIMPLEMENTED();
  return NULL;
}

void BackgroundContents::Close(RenderViewHost* render_view_host) {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host->process()->browser_context());
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_CLOSED,
      Source<Profile>(profile),
      Details<BackgroundContents>(this));
  delete this;
}

void BackgroundContents::RenderViewGone(RenderViewHost* rvh,
                                        base::TerminationStatus status,
                                        int error_code) {
  Profile* profile =
      Profile::FromBrowserContext(rvh->process()->browser_context());
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_TERMINATED,
      Source<Profile>(profile),
      Details<BackgroundContents>(this));

  // Our RenderView went away, so we should go away also, so killing the process
  // via the TaskManager doesn't permanently leave a BackgroundContents hanging
  // around the system, blocking future instances from being created
  // (http://crbug.com/65189).
  delete this;
}

RendererPreferences BackgroundContents::GetRendererPrefs(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  RendererPreferences preferences;
  renderer_preferences_util::UpdateFromSystemSettings(&preferences, profile);
  return preferences;
}

WebPreferences BackgroundContents::GetWebkitPrefs() {
  // TODO(rafaelw): Consider enabling the webkit_prefs.dom_paste_enabled for
  // apps.
  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->process()->browser_context());
  WebPreferences prefs = RenderViewHostDelegateHelper::GetWebkitPrefs(profile,
                                                                      false);
  // Disable all kinds of acceleration for background pages.
  // See http://crbug.com/96005 and http://crbug.com/96006
  prefs.force_compositing_mode = false;
  prefs.accelerated_compositing_enabled = false;
  prefs.accelerated_2d_canvas_enabled = false;
  prefs.accelerated_video_enabled = false;
  prefs.accelerated_drawing_enabled = false;
  prefs.accelerated_plugins_enabled = false;

  return prefs;
}

void BackgroundContents::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->process()->browser_context());
  delegate_view_helper_.CreateNewWindow(
      route_id,
      profile,
      render_view_host_->site_instance(),
      ChromeWebUIFactory::GetInstance()->GetWebUIType(profile, url_),
      this,
      params.window_container_type,
      params.frame_name);
}

void BackgroundContents::CreateNewWidget(int route_id,
                                         WebKit::WebPopupType popup_type) {
  NOTREACHED();
}

void BackgroundContents::CreateNewFullscreenWidget(int route_id) {
  NOTREACHED();
}

void BackgroundContents::ShowCreatedWindow(int route_id,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_pos,
                                           bool user_gesture) {
  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents)
    delegate_->AddTabContents(contents, disposition, initial_pos, user_gesture);
}

void BackgroundContents::ShowCreatedWidget(int route_id,
                                           const gfx::Rect& initial_pos) {
  NOTIMPLEMENTED();
}

void BackgroundContents::ShowCreatedFullscreenWidget(int route_id) {
  NOTIMPLEMENTED();
}

// static
BackgroundContents*
BackgroundContents::GetBackgroundContentsByID(int render_process_id,
                                              int render_view_id) {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  return render_view_host->delegate()->GetAsBackgroundContents();
}
