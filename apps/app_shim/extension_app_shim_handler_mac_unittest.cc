// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/extension_app_shim_handler_mac.h"

#include "apps/app_shim/app_shim_host_mac.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class TestingExtensionAppShimHandler : public ExtensionAppShimHandler {
 public:
  explicit TestingExtensionAppShimHandler() : fails_launch_(false) {}
  virtual ~TestingExtensionAppShimHandler() {}

  void set_fails_launch(bool fails_launch) {
    fails_launch_ = fails_launch;
  }

  AppShimHandler::Host* FindHost(Profile* profile,
                                 const std::string& app_id) {
    HostMap::const_iterator it = hosts().find(make_pair(profile, app_id));
    return it == hosts().end() ? NULL : it->second;
  }

  content::NotificationRegistrar& GetRegistrar() { return registrar(); }

 protected:
  virtual bool LaunchApp(Profile* profile,
                         const std::string& app_id,
                         AppShimLaunchType launch_type) OVERRIDE {
    return !fails_launch_;
  }

 private:
  bool fails_launch_;

  DISALLOW_COPY_AND_ASSIGN(TestingExtensionAppShimHandler);
};

const char kTestAppIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestAppIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class FakeHost : public apps::AppShimHandler::Host {
 public:
  FakeHost(Profile* profile,
           std::string app_id,
           TestingExtensionAppShimHandler* handler)
      : profile_(profile),
        app_id_(app_id),
        handler_(handler),
        close_count_(0) {}

  virtual void OnAppClosed() OVERRIDE {
    handler_->OnShimClose(this);
    ++close_count_;
  }
  virtual Profile* GetProfile() const OVERRIDE { return profile_; }
  virtual std::string GetAppId() const OVERRIDE { return app_id_; }

  int close_count() { return close_count_; }

 private:
  Profile* profile_;
  std::string app_id_;
  TestingExtensionAppShimHandler* handler_;
  int close_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeHost);
};

class ExtensionAppShimHandlerTest : public testing::Test {
 protected:
  ExtensionAppShimHandlerTest()
      : handler_(new TestingExtensionAppShimHandler),
        host_aa_(&profile_a_, kTestAppIdA, handler_.get()),
        host_ab_(&profile_a_, kTestAppIdB, handler_.get()),
        host_bb_(&profile_b_, kTestAppIdB, handler_.get()),
        host_aa_duplicate_(&profile_a_, kTestAppIdA, handler_.get()) {}

  scoped_ptr<TestingExtensionAppShimHandler> handler_;
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
  handler_->set_fails_launch(true);
  EXPECT_EQ(false, handler_->OnShimLaunch(&host_aa_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal startup.
  handler_->set_fails_launch(false);
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_aa_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(handler_->GetRegistrar().IsRegistered(
      handler_.get(),
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(&profile_a_)));
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_ab_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_ab_, handler_->FindHost(&profile_a_, kTestAppIdB));
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_bb_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_bb_, handler_->FindHost(&profile_b_, kTestAppIdB));
  EXPECT_TRUE(handler_->GetRegistrar().IsRegistered(
      handler_.get(),
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(&profile_b_)));

  // Starting and closing a second host does nothing.
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
