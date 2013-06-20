// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/extension_app_shim_handler_mac.h"

#include "apps/app_shim/app_shim_host_mac.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using ::testing::Return;

class MockProfileManagerFacade
    : public ExtensionAppShimHandler::ProfileManagerFacade {
 public:
  virtual ~MockProfileManagerFacade() {}

  MOCK_METHOD1(ProfileExistsForPath, bool(const base::FilePath& path));
  MOCK_METHOD1(ProfileForPath, Profile*(const base::FilePath& path));
};

class TestingExtensionAppShimHandler : public ExtensionAppShimHandler {
 public:
  TestingExtensionAppShimHandler(ProfileManagerFacade* profile_manager_facade) {
    set_profile_manager_facade(profile_manager_facade);
  }
  virtual ~TestingExtensionAppShimHandler() {}

  MOCK_METHOD3(LaunchApp, bool(Profile*,
                               const std::string&,
                               AppShimLaunchType));

  AppShimHandler::Host* FindHost(Profile* profile,
                                 const std::string& app_id) {
    HostMap::const_iterator it = hosts().find(make_pair(profile, app_id));
    return it == hosts().end() ? NULL : it->second;
  }

  content::NotificationRegistrar& GetRegistrar() { return registrar(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingExtensionAppShimHandler);
};

const char kTestAppIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestAppIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class FakeHost : public apps::AppShimHandler::Host {
 public:
  FakeHost(const base::FilePath& profile_path,
           const std::string& app_id,
           TestingExtensionAppShimHandler* handler)
      : profile_path_(profile_path),
        app_id_(app_id),
        handler_(handler),
        close_count_(0) {}

  virtual void OnAppClosed() OVERRIDE {
    handler_->OnShimClose(this);
    ++close_count_;
  }
  virtual base::FilePath GetProfilePath() const OVERRIDE {
    return profile_path_;
  }
  virtual std::string GetAppId() const OVERRIDE { return app_id_; }

  int close_count() { return close_count_; }

 private:
  base::FilePath profile_path_;
  std::string app_id_;
  TestingExtensionAppShimHandler* handler_;
  int close_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeHost);
};

class ExtensionAppShimHandlerTest : public testing::Test {
 protected:
  ExtensionAppShimHandlerTest()
      : profile_manager_facade_(new MockProfileManagerFacade),
        handler_(new TestingExtensionAppShimHandler(profile_manager_facade_)),
        profile_path_a_("Profile A"),
        profile_path_b_("Profile B"),
        host_aa_(profile_path_a_, kTestAppIdA, handler_.get()),
        host_ab_(profile_path_a_, kTestAppIdB, handler_.get()),
        host_bb_(profile_path_b_, kTestAppIdB, handler_.get()),
        host_aa_duplicate_(profile_path_a_, kTestAppIdA, handler_.get()) {
    EXPECT_CALL(*profile_manager_facade_, ProfileExistsForPath(profile_path_a_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_manager_facade_, ProfileForPath(profile_path_a_))
        .WillRepeatedly(Return(&profile_a_));
    EXPECT_CALL(*profile_manager_facade_, ProfileExistsForPath(profile_path_b_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_manager_facade_, ProfileForPath(profile_path_b_))
        .WillRepeatedly(Return(&profile_b_));
  }

  MockProfileManagerFacade* profile_manager_facade_;
  scoped_ptr<TestingExtensionAppShimHandler> handler_;
  base::FilePath profile_path_a_;
  base::FilePath profile_path_b_;
  TestingProfile profile_a_;
  TestingProfile profile_b_;
  FakeHost host_aa_;
  FakeHost host_ab_;
  FakeHost host_bb_;
  FakeHost host_aa_duplicate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandlerTest);
};

TEST_F(ExtensionAppShimHandlerTest, LaunchAndCloseShim) {
  // If launch fails, the host is not added to the map.
  EXPECT_CALL(*handler_,
              LaunchApp(&profile_a_, kTestAppIdA, APP_SHIM_LAUNCH_NORMAL))
      .WillOnce(Return(false));
  EXPECT_EQ(false, handler_->OnShimLaunch(&host_aa_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal startup.
  EXPECT_CALL(*handler_,
              LaunchApp(&profile_a_, kTestAppIdA, APP_SHIM_LAUNCH_NORMAL))
      .WillOnce(Return(true));
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_aa_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(handler_->GetRegistrar().IsRegistered(
      handler_.get(),
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(&profile_a_)));

  EXPECT_CALL(*handler_,
              LaunchApp(&profile_a_, kTestAppIdB, APP_SHIM_LAUNCH_NORMAL))
      .WillOnce(Return(true));
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_ab_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_ab_, handler_->FindHost(&profile_a_, kTestAppIdB));

  EXPECT_CALL(*handler_,
              LaunchApp(&profile_b_, kTestAppIdB, APP_SHIM_LAUNCH_NORMAL))
      .WillOnce(Return(true));
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_bb_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_bb_, handler_->FindHost(&profile_b_, kTestAppIdB));
  EXPECT_TRUE(handler_->GetRegistrar().IsRegistered(
      handler_.get(),
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(&profile_b_)));

  // Starting and closing a second host does nothing.
  EXPECT_CALL(*handler_,
              LaunchApp(&profile_a_, kTestAppIdA, APP_SHIM_LAUNCH_NORMAL))
      .WillOnce(Return(false));
  EXPECT_EQ(false, handler_->OnShimLaunch(&host_aa_duplicate_,
                                          APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));
  handler_->OnShimClose(&host_aa_duplicate_);
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  handler_->OnShimClose(&host_aa_);
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Closing the second host afterward does nothing.
  handler_->OnShimClose(&host_aa_duplicate_);
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Destruction sends OnAppClose to remaining hosts.
  handler_.reset();
  EXPECT_EQ(1, host_ab_.close_count());
  EXPECT_EQ(1, host_bb_.close_count());
}

}  // namespace apps
