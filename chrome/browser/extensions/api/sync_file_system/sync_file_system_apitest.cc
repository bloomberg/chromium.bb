// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "webkit/quota/quota_manager.h"

namespace chrome {

namespace {

class SyncFileSystemApiTest : public ExtensionApiTest {
 public:
  // Override the current channel to "trunk" as syncFileSystem is currently
  // available only on trunk channel.
  SyncFileSystemApiTest()
      : current_channel_(VersionInfo::CHANNEL_UNKNOWN) {
  }

  void SetUp() {
    // TODO(calvinlo): Update test code after default quota is made const
    // (http://crbug.com/155488).
    real_default_quota_ = quota::QuotaManager::kSyncableStorageDefaultHostQuota;
    quota::QuotaManager::kSyncableStorageDefaultHostQuota = 123456789;
  }

  void TearDown() {
    quota::QuotaManager::kSyncableStorageDefaultHostQuota = real_default_quota_;
  }

 private:
  extensions::Feature::ScopedCurrentChannel current_channel_;
  int64 real_default_quota_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetUsageAndQuota) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_usage_and_quota"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, RequestFileSystem) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/request_file_system"))
      << message_;
}

}  // namespace chrome
