// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/system_util_cleaner.h"

#include <windows.h>

#include <shlwapi.h>
#include <stdint.h>
#include <wincrypt.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

class ServiceUtilCleanerTest : public testing::Test {
 public:
  void SetUp() override {
    // Cleanup previous run. This may happen when previous execution of unittest
    // crashed, leaving background processes/services.
    if (IsProcessRunning(kTestServiceExecutableName)) {
      StopService(kServiceName);
      WaitForProcessesStopped(kTestServiceExecutableName);
    }
    DeleteService(kServiceName);
    ASSERT_TRUE(WaitForServiceDeleted(kServiceName));

    ASSERT_FALSE(IsProcessRunning(kTestServiceExecutableName));
    ASSERT_FALSE(DoesServiceExist(kServiceName));
  }
};

}  // namespace

TEST(SystemUtilCleanerTests, AcquireDebugRightsPrivileges) {
  ASSERT_FALSE(HasDebugRightsPrivileges());
  EXPECT_TRUE(AcquireDebugRightsPrivileges());
  EXPECT_TRUE(HasDebugRightsPrivileges());
  EXPECT_TRUE(ReleaseDebugRightsPrivileges());
  EXPECT_FALSE(HasDebugRightsPrivileges());
}

TEST(SystemUtilCleanerTests, OpenRegistryKeyWithInvalidParameter) {
  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, L"non-existing key path");
  base::win::RegKey key;
  EXPECT_FALSE(key_path.Open(KEY_READ, &key));
}

TEST_F(ServiceUtilCleanerTest, DeleteService) {
  TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  service_handle.Close();

  EXPECT_TRUE(DoesServiceExist(service_handle.service_name()));
  EXPECT_TRUE(DeleteService(service_handle.service_name()));
  EXPECT_TRUE(WaitForServiceDeleted(service_handle.service_name()));
  EXPECT_FALSE(DoesServiceExist(service_handle.service_name()));
}

// Flaky. https://crbug.com/871784
TEST_F(ServiceUtilCleanerTest, FLAKY_StopAndDeleteRunningService) {
  // Install and launch the service.
  TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  ASSERT_TRUE(service_handle.StartService());
  EXPECT_TRUE(DoesServiceExist(service_handle.service_name()));
  EXPECT_TRUE(IsProcessRunning(kTestServiceExecutableName));
  service_handle.Close();

  // Stop the service.
  EXPECT_TRUE(StopService(service_handle.service_name()));
  EXPECT_TRUE(WaitForProcessesStopped(kTestServiceExecutableName));
  EXPECT_TRUE(WaitForServiceStopped(service_handle.service_name()));

  // Delete the service
  EXPECT_TRUE(DeleteService(service_handle.service_name()));
  EXPECT_TRUE(WaitForServiceDeleted(service_handle.service_name()));

  // The service must be fully stopped and deleted.
  EXPECT_FALSE(DoesServiceExist(service_handle.service_name()));
  EXPECT_FALSE(IsProcessRunning(kTestServiceExecutableName));
}

// Flaky. https://crbug.com/871784
TEST_F(ServiceUtilCleanerTest, FLAKY_DeleteRunningService) {
  // Install and launch the service.
  TestScopedServiceHandle service_handle;
  ASSERT_TRUE(service_handle.InstallService());
  ASSERT_TRUE(service_handle.StartService());
  EXPECT_TRUE(DoesServiceExist(service_handle.service_name()));
  EXPECT_TRUE(IsProcessRunning(kTestServiceExecutableName));
  service_handle.Close();

  // Delete the service
  EXPECT_TRUE(DeleteService(service_handle.service_name()));

  // The service must be fully stopped and deleted.
  EXPECT_TRUE(WaitForProcessesStopped(kTestServiceExecutableName));
  EXPECT_FALSE(DoesServiceExist(service_handle.service_name()));
  EXPECT_FALSE(IsProcessRunning(kTestServiceExecutableName));
}

}  // namespace chrome_cleaner
