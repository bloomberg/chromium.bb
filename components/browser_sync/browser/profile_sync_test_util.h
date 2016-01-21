// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_

#include "base/time/time.h"
#include "components/sync_driver/sync_service_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class Time;
class TimeDelta;
}

namespace browser_sync {

// An empty syncer::NetworkTimeUpdateCallback. Used in various tests to
// instantiate ProfileSyncService.
void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&);

class SyncServiceObserverMock : public sync_driver::SyncServiceObserver {
 public:
  SyncServiceObserverMock();
  virtual ~SyncServiceObserverMock();

  MOCK_METHOD0(OnStateChanged, void());
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_PROFILE_SYNC_TEST_UTIL_H_
