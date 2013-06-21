// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/extension_app_shim_handler_mac.h"

#include "apps/app_shim/app_shim_host_mac.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using extensions::Extension;
typedef extensions::ShellWindowRegistry::ShellWindowList ShellWindowList;

using ::testing::_;
using ::testing::Return;

class MockDelegate : public ExtensionAppShimHandler::Delegate {
 public:
  virtual ~MockDelegate() {}

  MOCK_METHOD1(ProfileExistsForPath, bool(const base::FilePath&));
  MOCK_METHOD1(ProfileForPath, Profile*(const base::FilePath&));

  MOCK_METHOD2(GetWindows, ShellWindowList(Profile*, const std::string&));

  MOCK_METHOD2(GetAppExtension, const Extension*(Profile*, const std::string&));
  MOCK_METHOD2(LaunchApp, void(Profile*, const Extension*));
  MOCK_METHOD2(LaunchShim, void(Profile*, const Extension*));

};

class TestingExtensionAppShimHandler : public ExtensionAppShimHandler {
 public:
  TestingExtensionAppShimHandler(Delegate* delegate) {
    set_delegate(delegate);
  }
  virtual ~TestingExtensionAppShimHandler() {}

  MOCK_METHOD1(OnShimFocus, void(Host* host));

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
      : delegate_(new MockDelegate),
        handler_(new TestingExtensionAppShimHandler(delegate_)),
        profile_path_a_("Profile A"),
        profile_path_b_("Profile B"),
        host_aa_(profile_path_a_, kTestAppIdA, handler_.get()),
        host_ab_(profile_path_a_, kTestAppIdB, handler_.get()),
        host_bb_(profile_path_b_, kTestAppIdB, handler_.get()),
        host_aa_duplicate_(profile_path_a_, kTestAppIdA, handler_.get()) {
    base::FilePath extension_path("/fake/path");
    base::DictionaryValue manifest;
    manifest.SetString("name", "Fake Name");
    manifest.SetString("version", "1");
    std::string error;
    extension_a_ = Extension::Create(
        extension_path, extensions::Manifest::INTERNAL, manifest,
        Extension::NO_FLAGS, kTestAppIdA, &error);
    EXPECT_TRUE(extension_a_.get()) << error;

    extension_b_ = Extension::Create(
        extension_path, extensions::Manifest::INTERNAL, manifest,
        Extension::NO_FLAGS, kTestAppIdB, &error);
    EXPECT_TRUE(extension_b_.get()) << error;

    EXPECT_CALL(*delegate_, ProfileExistsForPath(profile_path_a_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, ProfileForPath(profile_path_a_))
        .WillRepeatedly(Return(&profile_a_));
    EXPECT_CALL(*delegate_, ProfileExistsForPath(profile_path_b_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, ProfileForPath(profile_path_b_))
        .WillRepeatedly(Return(&profile_b_));

    // In most tests, we don't care about the result of GetWindows, it just
    // needs to be non-empty.
    ShellWindowList shell_window_list;
    shell_window_list.push_back(static_cast<ShellWindow*>(NULL));
    EXPECT_CALL(*delegate_, GetWindows(_, _))
        .WillRepeatedly(Return(shell_window_list));

    EXPECT_CALL(*delegate_, GetAppExtension(_, kTestAppIdA))
        .WillRepeatedly(Return(extension_a_.get()));
    EXPECT_CALL(*delegate_, GetAppExtension(_, kTestAppIdB))
        .WillRepeatedly(Return(extension_b_.get()));
    EXPECT_CALL(*delegate_, LaunchApp(_,_))
        .WillRepeatedly(Return());
  }

  MockDelegate* delegate_;
  scoped_ptr<TestingExtensionAppShimHandler> handler_;
  base::FilePath profile_path_a_;
  base::FilePath profile_path_b_;
  TestingProfile profile_a_;
  TestingProfile profile_b_;
  FakeHost host_aa_;
  FakeHost host_ab_;
  FakeHost host_bb_;
  FakeHost host_aa_duplicate_;
  scoped_refptr<Extension> extension_a_;
  scoped_refptr<Extension> extension_b_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandlerTest);
};

TEST_F(ExtensionAppShimHandlerTest, LaunchAndCloseShim) {
  const AppShimLaunchType normal_launch = APP_SHIM_LAUNCH_NORMAL;

  // If launch fails, the host is not added to the map.
  EXPECT_CALL(*delegate_, GetAppExtension(&profile_a_, kTestAppIdA))
      .WillOnce(Return(static_cast<const Extension*>(NULL)))
      .WillRepeatedly(Return(extension_a_.get()));
  EXPECT_EQ(false, handler_->OnShimLaunch(&host_aa_, normal_launch));
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal startup.
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_aa_, normal_launch));
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));

  EXPECT_EQ(true, handler_->OnShimLaunch(&host_ab_, normal_launch));
  EXPECT_EQ(&host_ab_, handler_->FindHost(&profile_a_, kTestAppIdB));

  EXPECT_EQ(true, handler_->OnShimLaunch(&host_bb_, normal_launch));
  EXPECT_EQ(&host_bb_, handler_->FindHost(&profile_b_, kTestAppIdB));

  // Starting and closing a second host just focuses the app.
  EXPECT_CALL(*handler_, OnShimFocus(&host_aa_duplicate_));
  EXPECT_EQ(false, handler_->OnShimLaunch(&host_aa_duplicate_, normal_launch));
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));
  handler_->OnShimClose(&host_aa_duplicate_);
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  handler_->OnShimClose(&host_aa_);
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Closing the second host afterward does nothing.
  handler_->OnShimClose(&host_aa_duplicate_);
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(ExtensionAppShimHandlerTest, AppLifetime) {
  EXPECT_CALL(*delegate_, LaunchShim(&profile_a_, extension_a_.get()));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);

  // Launch the shim, adding an entry in the map.
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_aa_, APP_SHIM_LAUNCH_NORMAL));
  EXPECT_EQ(&host_aa_, handler_->FindHost(&profile_a_, kTestAppIdA));

  handler_->OnAppDeactivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(1, host_aa_.close_count());
}

TEST_F(ExtensionAppShimHandlerTest, RegisterOnly) {
  // For an APP_SHIM_LAUNCH_REGISTER_ONLY, don't launch the app.
  EXPECT_CALL(*delegate_, LaunchApp(_, _))
      .Times(0);
  EXPECT_EQ(true, handler_->OnShimLaunch(&host_aa_,
                                          APP_SHIM_LAUNCH_REGISTER_ONLY));
  EXPECT_TRUE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Close the shim, removing the entry in the map.
  handler_->OnShimClose(&host_aa_);
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Don't register if there are no windows open for the app. This can happen if
  // the app shim was launched in response to an app being activated, but the
  // app was deactivated before the shim is registered.
  ShellWindowList shell_window_list;
  EXPECT_CALL(*delegate_, GetWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(shell_window_list));
  EXPECT_EQ(false, handler_->OnShimLaunch(&host_aa_,
                                          APP_SHIM_LAUNCH_REGISTER_ONLY));
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

}  // namespace apps
