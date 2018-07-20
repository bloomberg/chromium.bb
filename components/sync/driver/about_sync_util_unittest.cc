// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/about_sync_util.h"

#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/engine/sync_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace sync_ui_util {
namespace {

class SyncServiceMock : public FakeSyncService {
 public:
  bool IsFirstSetupComplete() const override { return true; }

  int GetDisableReasons() const override {
    return DISABLE_REASON_UNRECOVERABLE_ERROR;
  }

  bool QueryDetailedSyncStatus(SyncStatus* result) const override {
    return false;
  }

  SyncCycleSnapshot GetLastCycleSnapshot() const override {
    return SyncCycleSnapshot();
  }
};

TEST(SyncUIUtilTestAbout, ConstructAboutInformationWithUnrecoverableErrorTest) {
  SyncServiceMock service;

  std::unique_ptr<base::DictionaryValue> strings(
      ConstructAboutInformation(&service, version_info::Channel::UNKNOWN));

  EXPECT_TRUE(strings->HasKey("unrecoverable_error_detected"));
}

}  // namespace
}  // namespace sync_ui_util
}  // namespace syncer
