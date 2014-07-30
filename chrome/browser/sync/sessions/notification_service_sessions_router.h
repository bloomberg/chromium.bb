// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_NOTIFICATION_SERVICE_SESSIONS_ROUTER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_NOTIFICATION_SERVICE_SESSIONS_ROUTER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class WebContents;
}

namespace browser_sync {

// A SessionsSyncManager::LocalEventRouter that drives session sync via
// the NotificationService.
class NotificationServiceSessionsRouter
    : public LocalSessionEventRouter,
      public content::NotificationObserver {
 public:
  NotificationServiceSessionsRouter(
      Profile* profile,
      const syncer::SyncableService::StartSyncFlare& flare);
  virtual ~NotificationServiceSessionsRouter();

  // content::NotificationObserver implementation.
  // BrowserSessionProvider -> sync API model change application.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SessionsSyncManager::LocalEventRouter implementation.
  virtual void StartRoutingTo(LocalSessionEventHandler* handler) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // Called when the URL visited in |web_contents| was blocked by the
  // SupervisedUserService. We forward this on to our handler_ via the
  // normal OnLocalTabModified, but pass through here via a WeakPtr
  // callback from SupervisedUserService and to extract the tab delegate
  // from WebContents.
  void OnNavigationBlocked(content::WebContents* web_contents);

  LocalSessionEventHandler* handler_;
  content::NotificationRegistrar registrar_;
  Profile* const profile_;
  syncer::SyncableService::StartSyncFlare flare_;
  base::WeakPtrFactory<NotificationServiceSessionsRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationServiceSessionsRouter);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SESSIONS_NOTIFICATION_SERVICE_SESSIONS_ROUTER_H_
