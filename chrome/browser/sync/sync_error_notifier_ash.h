// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_ASH_H_
#define CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_ASH_H_

#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/driver/sync_service_observer.h"

class Profile;

// Shows sync-related errors as notifications in Ash.
class SyncErrorNotifier : public syncer::SyncServiceObserver,
                          public KeyedService {
 public:
  SyncErrorNotifier(syncer::SyncService* sync_service, Profile* profile);
  ~SyncErrorNotifier() override;

  // KeyedService:
  void Shutdown() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* service) override;

 private:
  // The sync service to query for error details.
  syncer::SyncService* sync_service_;

  // The Profile this service belongs to.
  Profile* const profile_;

  // Notification was added to NotificationUIManager. This flag is used to
  // prevent displaying passphrase notification to user if they already saw (and
  // potentially dismissed) previous one.
  bool notification_displayed_ = false;

  // Used to keep track of the message center notification.
  std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifier);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_ASH_H_
