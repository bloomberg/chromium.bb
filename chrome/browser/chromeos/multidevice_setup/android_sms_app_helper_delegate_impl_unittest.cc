// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/android_sms_app_helper_delegate_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class AndroidSmsAppHelperDelegateImplTest : public testing::Test {
 protected:
  AndroidSmsAppHelperDelegateImplTest() = default;
  ~AndroidSmsAppHelperDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pending_app_manager_ =
        std::make_unique<web_app::TestPendingAppManager>();
    android_sms_app_helper_delegate_ = base::WrapUnique(
        new AndroidSmsAppHelperDelegateImpl(test_pending_app_manager_.get()));
  }

  web_app::TestPendingAppManager* test_pending_app_manager() {
    return test_pending_app_manager_.get();
  }

  void InstallApp() {
    android_sms_app_helper_delegate_->InstallAndroidSmsApp();
  }

 private:
  std::unique_ptr<web_app::TestPendingAppManager> test_pending_app_manager_;
  std::unique_ptr<AndroidSmsAppHelperDelegate> android_sms_app_helper_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImplTest);
};

TEST_F(AndroidSmsAppHelperDelegateImplTest, TestInstallMessagesApp) {
  InstallApp();

  std::vector<web_app::PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(web_app::PendingAppManager::AppInfo(
      chromeos::android_sms::GetAndroidMessagesURLWithExperiments(),
      web_app::PendingAppManager::LaunchContainer::kWindow,
      web_app::PendingAppManager::InstallSource::kDefaultInstalled));
  EXPECT_EQ(expected_apps_to_install,
            test_pending_app_manager()->installed_apps());
}

}  // namespace multidevice_setup

}  // namespace chromeos
