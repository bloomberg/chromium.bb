// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
#pragma once

#include <string>

#include "chrome/browser/sync/syncable/model_type_payload_map.h"

namespace sync_notifier {

class SyncNotifierObserver {
 public:
  SyncNotifierObserver() {}
  virtual ~SyncNotifierObserver() {}

  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads) = 0;
  virtual void OnNotificationStateChange(bool notifications_enabled) = 0;

  // TODO(nileshagrawal): Find a way to hide state handling inside the
  // sync notifier implementation.
  virtual void StoreState(const std::string& state) = 0;
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_OBSERVER_H_
