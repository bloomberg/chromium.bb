// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSyncServiceFactoryTest : public testing::Test {
 protected:
  ProfileSyncServiceFactoryTest() {}

  void SetUp() override {
    profile_.reset(new TestingProfile());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
};

// Verify that the disable sync flag disables creation of the sync service.
TEST_F(ProfileSyncServiceFactoryTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  EXPECT_EQ(nullptr, ProfileSyncServiceFactory::GetForProfile(profile_.get()));
}
