// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"

#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(sync_sessions::SyncSessionsRouterTabHelper);

namespace sync_sessions {

// static
void SyncSessionsRouterTabHelper::CreateForWebContents(
    content::WebContents* web_contents,
    SyncSessionsWebContentsRouter* router) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), new SyncSessionsRouterTabHelper(web_contents, router));
  }
}

SyncSessionsRouterTabHelper::SyncSessionsRouterTabHelper(
    content::WebContents* web_contents,
    SyncSessionsWebContentsRouter* router)
    : content::WebContentsObserver(web_contents),
      router_(router),
      source_tab_id_(kInvalidTabID) {}

SyncSessionsRouterTabHelper::~SyncSessionsRouterTabHelper() {}

void SyncSessionsRouterTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::TitleWasSet(content::NavigationEntry* entry,
                                              bool explicit_set) {
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::WebContentsDestroyed() {
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  SessionID::id_type source_tab_id = SessionTabHelper::IdForTab(web_contents());
  if (new_contents &&
      SyncSessionsRouterTabHelper::FromWebContents(new_contents) &&
      new_contents != web_contents() && source_tab_id != kInvalidTabID) {
    SyncSessionsRouterTabHelper::FromWebContents(new_contents)
        ->set_source_tab_id(source_tab_id);
  }
  NotifyRouter();
}

void SyncSessionsRouterTabHelper::NotifyRouter() {
  if (router_)
    router_->NotifyTabModified(web_contents());
}

}  // namespace sync_sessions
