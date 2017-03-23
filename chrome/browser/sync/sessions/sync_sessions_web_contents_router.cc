// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#if !defined(OS_ANDROID)
#include "chrome/browser/sync/sessions/browser_list_router_helper.h"
#endif  // !defined(OS_ANDROID)
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

SyncSessionsWebContentsRouter::SyncSessionsWebContentsRouter(Profile* profile) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    favicon_changed_subscription_ = history_service->AddFaviconsChangedCallback(
        base::Bind(&SyncSessionsWebContentsRouter::OnFaviconsChanged,
                   base::Unretained(this)));
  }

#if !defined(OS_ANDROID)
  browser_list_helper_ = base::MakeUnique<BrowserListRouterHelper>(this);
#endif  // !defined(OS_ANDROID)
}

SyncSessionsWebContentsRouter::~SyncSessionsWebContentsRouter() {}

void SyncSessionsWebContentsRouter::NotifyTabModified(
    content::WebContents* web_contents) {
  if (handler_ && web_contents) {
    SyncedTabDelegate* delegate =
        TabContentsSyncedTabDelegate::FromWebContents(web_contents);
    if (delegate)
      handler_->OnLocalTabModified(delegate);
  }

  if (!flare_.is_null()) {
    flare_.Run(syncer::SESSIONS);
    flare_.Reset();
  }
}

void SyncSessionsWebContentsRouter::InjectStartSyncFlare(
    syncer::SyncableService::StartSyncFlare flare) {
  flare_ = flare;
}

void SyncSessionsWebContentsRouter::StartRoutingTo(
    LocalSessionEventHandler* handler) {
  handler_ = handler;
}

void SyncSessionsWebContentsRouter::Stop() {
  handler_ = nullptr;
}

void SyncSessionsWebContentsRouter::OnFaviconsChanged(
    const std::set<GURL>& page_urls,
    const GURL& icon_url) {
  if (handler_)
    handler_->OnFaviconsChanged(page_urls, icon_url);
}

void SyncSessionsWebContentsRouter::Shutdown() {
  favicon_changed_subscription_.reset();

#if !defined(OS_ANDROID)
  browser_list_helper_.reset();
#endif  // !defined(OS_ANDROID)
}

}  // namespace sync_sessions
