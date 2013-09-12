// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_integration_service.h"

#include "chrome/browser/chromeos/drive/dummy_file_system.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/dummy_drive_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveIntegrationServiceTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    integration_service_.reset(new DriveIntegrationService(
        profile_.get(), NULL,
        new DummyDriveService, base::FilePath(), new DummyFileSystem));
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DriveIntegrationService> integration_service_;
};

TEST_F(DriveIntegrationServiceTest, InitializeAndShutdown) {
  integration_service_->SetEnabled(true);
  test_util::RunBlockingPoolTask();
  integration_service_->Shutdown();
}

}  // namespace drive
