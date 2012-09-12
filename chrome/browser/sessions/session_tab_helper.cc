// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_tab_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

int SessionTabHelper::kUserDataKey;

SessionTabHelper::SessionTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents) {
}

SessionTabHelper::~SessionTabHelper() {
}

void SessionTabHelper::SetWindowID(const SessionID& id) {
  window_id_ = id;

  // TODO(mpcomplete): Maybe this notification should send out a WebContents.
  TabContents* tab = TabContents::FromWebContents(web_contents());
  if (tab) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TAB_PARENTED,
        content::Source<TabContents>(tab),
        content::NotificationService::NoDetails());
  }

  // Extension code in the renderer holds the ID of the window that hosts it.
  // Notify it that the window ID changed.
  web_contents()->GetRenderViewHost()->Send(
          new ExtensionMsg_UpdateBrowserWindowId(
          web_contents()->GetRenderViewHost()->GetRoutingID(), id.id()));
}

void SessionTabHelper::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  render_view_host->Send(
      new ExtensionMsg_UpdateBrowserWindowId(render_view_host->GetRoutingID(),
                                             window_id_.id()));
}

void SessionTabHelper::UserAgentOverrideSet(const std::string& user_agent) {
#if defined(ENABLE_SESSION_SERVICE)
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SessionService* session = SessionServiceFactory::GetForProfile(profile);
  if (session)
    session->SetTabUserAgentOverride(window_id(), session_id(), user_agent);
#endif
}
