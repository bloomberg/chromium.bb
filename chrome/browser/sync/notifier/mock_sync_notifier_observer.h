// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_MOCK_SYNC_NOTIFIER_OBSERVER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_MOCK_SYNC_NOTIFIER_OBSERVER_H_
#pragma once

#include <string>

#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_notifier {

class MockSyncNotifierObserver : public SyncNotifierObserver {
 public:
  MockSyncNotifierObserver();
  virtual ~MockSyncNotifierObserver();

  MOCK_METHOD1(OnIncomingNotification,
               void(const syncable::ModelTypePayloadMap&));
  MOCK_METHOD1(OnNotificationStateChange, void(bool));
  MOCK_METHOD1(StoreState, void(const std::string&));
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_MOCK_SYNC_NOTIFIER_OBSERVER_H_
