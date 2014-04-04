// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_integration_service.h"

#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/dummy_drive_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace{
const char kTestProfileName[] = "test-profile";
}

class DriveIntegrationServiceTest : public testing::Test {
 public:
  DriveIntegrationServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(profile_manager_.SetUp());
    integration_service_.reset(new DriveIntegrationService(
        profile_manager_.CreateTestingProfile(kTestProfileName),
        NULL,
        new DummyDriveService,
        std::string(),
        base::FilePath(),
        new DummyFileSystem));
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  // DriveIntegrationService depends on DriveNotificationManager which depends
  // on InvalidationService. On Chrome OS, the InvalidationServiceFactory
  // uses chromeos::ProfileHelper, which needs the ProfileManager or a
  // TestProfileManager to be running.
  TestingProfileManager profile_manager_;
  scoped_ptr<DriveIntegrationService> integration_service_;
};

TEST_F(DriveIntegrationServiceTest, InitializeAndShutdown) {
  integration_service_->SetEnabled(true);
  test_util::RunBlockingPoolTask();
  integration_service_->Shutdown();
}

}  // namespace drive
