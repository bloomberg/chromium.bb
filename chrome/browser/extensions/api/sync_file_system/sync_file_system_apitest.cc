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

 private:
  extensions::Feature::ScopedCurrentChannel current_channel_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetUsageAndQuota) {
  // TODO(calvinlo): Check value against mock syncFileSystem quota value instead
  // of comparing against the default. (http://crbug.com/153501).
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_usage_and_quota"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, RequestFileSystem) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/request_file_system"))
      << message_;
}

}  // namespace chrome
