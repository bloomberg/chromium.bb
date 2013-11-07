// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_keep_alive_service.h"
#include "apps/app_keep_alive_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)

class AppKeepAliveServiceUnitTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    service_.reset(new apps::AppKeepAliveService(&profile_));
  }

  virtual void TearDown() OVERRIDE {
    while (chrome::WillKeepAlive())
      chrome::EndKeepAlive();
    testing::Test::TearDown();
  }

  TestingProfile profile_;
  scoped_ptr<apps::AppKeepAliveService> service_;
};

TEST_F(AppKeepAliveServiceUnitTest, Basic) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}

// Test that apps running in different profiles are ignored.
TEST_F(AppKeepAliveServiceUnitTest, DifferentProfile) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(NULL, "foo");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(NULL, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}

// Test that OnAppStop without a prior corresponding OnAppStart is ignored.
TEST_F(AppKeepAliveServiceUnitTest, StopAppBeforeOpening) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}

// Test that OnAppStart for an app that has already started is ignored.
TEST_F(AppKeepAliveServiceUnitTest, StartMoreThanOnce) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}

// Test that OnAppStart is ignored after the service has been shut down.
TEST_F(AppKeepAliveServiceUnitTest, StartAfterShutdown) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  service_->OnAppStart(&profile_, "foo");
  EXPECT_FALSE(chrome::WillKeepAlive());
}

TEST_F(AppKeepAliveServiceUnitTest, MultipleApps) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "bar");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "bar");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}

// Test that all keep alives are ended when OnChromeTerminating is called.
TEST_F(AppKeepAliveServiceUnitTest, ChromeTerminateWithAppsStarted) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "bar");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnChromeTerminating();
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStop(&profile_, "foo");
  service_->OnAppStop(&profile_, "bar");
  EXPECT_FALSE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}

// Test that all keep alives are ended when Shutdown is called.
TEST_F(AppKeepAliveServiceUnitTest, ProfileShutdownWithAppsStarted) {
  ASSERT_FALSE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "foo");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->OnAppStart(&profile_, "bar");
  EXPECT_TRUE(chrome::WillKeepAlive());
  service_->Shutdown();
  EXPECT_FALSE(chrome::WillKeepAlive());
}
#endif
