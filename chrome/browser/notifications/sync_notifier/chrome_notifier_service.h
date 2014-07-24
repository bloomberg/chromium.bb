// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class SyncedNotificationsShim;

namespace extensions {
struct Event;
}

namespace notifier {

// The ChromeNotifierService acts as the bridge between sync and the JS
// extension that implements the synced notifications processing code.
// TODO(zea): consider making this a generic bridge for extension backed
// datatypes.
class ChromeNotifierService : public KeyedService {
 public:
  explicit ChromeNotifierService(Profile* profile);
  virtual ~ChromeNotifierService();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Returns the SyncableService for syncer::SYNCED_NOTIFICATIONS and
  // syncer::SYNCED_NOTIFICATION_APP_INFO
  SyncedNotificationsShim* GetSyncedNotificationsShim();

 private:
  // Helper method for firing JS events triggered by sync.
  void FireSyncJSEvent(scoped_ptr<extensions::Event> event);

  // Helper method for trigger synced notification refreshes.
  void NotifyRefreshNeeded();

  // The owning profile.
  Profile* const profile_;

  // Shim connecting the JS private api to sync. // TODO(zea): delete all other
  // code.
  scoped_ptr<SyncedNotificationsShim> synced_notifications_shim_;

  base::WeakPtrFactory<ChromeNotifierService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierService);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
