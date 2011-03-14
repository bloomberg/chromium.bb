// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface to the sync notifier, which is an object that receives
// notifications when updates are available for a set of sync types.
// All the observers are notified when such an event happens.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_H_

#include <string>

#include "chrome/browser/sync/syncable/model_type.h"

namespace sync_notifier {
class SyncNotifierObserver;

class SyncNotifier {
 public:
  SyncNotifier() {}
  virtual ~SyncNotifier() {}

  virtual void AddObserver(SyncNotifierObserver* observer) = 0;
  virtual void RemoveObserver(SyncNotifierObserver* observer) = 0;

  // SetState must be called once, before any call to UpdateCredentials.
  virtual void SetState(const std::string& state) = 0;

  // The observers won't be notified of any notifications until
  // UpdateCredentials is called at least once. It can be called more than
  // once.
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) = 0;

  virtual void UpdateEnabledTypes(const syncable::ModelTypeSet& types) = 0;

  // This is here only to support the old p2p notification implementation,
  // which is still used by sync integration tests.
  // TODO(akalin): Remove this once we move the integration tests off p2p
  // notifications.
  virtual void SendNotification() = 0;
};
}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_H_

