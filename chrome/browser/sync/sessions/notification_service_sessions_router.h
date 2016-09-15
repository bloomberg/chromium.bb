// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_NOTIFICATION_SERVICE_SESSIONS_ROUTER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_NOTIFICATION_SERVICE_SESSIONS_ROUTER_H_

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace sync_sessions {

class SyncSessionsClient;

// A SessionsSyncManager::LocalEventRouter that drives session sync via
// the NotificationService.
class NotificationServiceSessionsRouter
    : public LocalSessionEventRouter,
      public content::NotificationObserver {
 public:
  NotificationServiceSessionsRouter(
      Profile* profile,
      SyncSessionsClient* sessions_client_,
      const syncer::SyncableService::StartSyncFlare& flare);
  ~NotificationServiceSessionsRouter() override;

  // content::NotificationObserver implementation.
  // BrowserSessionProvider -> sync API model change application.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // SessionsSyncManager::LocalEventRouter implementation.
  void StartRoutingTo(LocalSessionEventHandler* handler) override;
  void Stop() override;

 private:
  // Called when the URL visited in |web_contents| was blocked by the
  // SupervisedUserService. We forward this on to our handler_ via the
  // normal OnLocalTabModified, but pass through here via a WeakPtr
  // callback from SupervisedUserService and to extract the tab delegate
  // from WebContents.
  void OnNavigationBlocked(content::WebContents* web_contents);

  // Called when the favicons for the given page URLs
  // (e.g. http://www.google.com) and the given icon URL (e.g.
  // http://www.google.com/favicon.ico) have changed. It is valid to call
  // OnFaviconsChanged() with non-empty |page_urls| and an empty |icon_url|
  // and vice versa.
  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url);

  LocalSessionEventHandler* handler_;
  content::NotificationRegistrar registrar_;
  Profile* const profile_;
  SyncSessionsClient* const sessions_client_;
  syncer::SyncableService::StartSyncFlare flare_;

  std::unique_ptr<base::CallbackList<void(const std::set<GURL>&,
                                          const GURL&)>::Subscription>
      favicon_changed_subscription_;

  base::WeakPtrFactory<NotificationServiceSessionsRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationServiceSessionsRouter);
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_NOTIFICATION_SERVICE_SESSIONS_ROUTER_H_
