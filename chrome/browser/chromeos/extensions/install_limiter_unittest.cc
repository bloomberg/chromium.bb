// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/install_limiter.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::InstallLimiter;

namespace {

constexpr char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";
constexpr int kLargeExtensionSize = 2000000;
constexpr int kSmallExtensionSize = 200000;

}  // namespace

class InstallLimiterTest : public testing::Test {
 public:
  InstallLimiterTest() = default;
  ~InstallLimiterTest() override = default;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::ScopedStubInstallAttributes test_install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(InstallLimiterTest);
};

TEST_F(InstallLimiterTest, ShouldDeferInstall) {
  // In non-demo mode, all apps larger than 1 MB should be deferred.
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kNone);
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(
      kLargeExtensionSize, extension_misc::kScreensaverAppId));
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));

  // In demo mode (either online or offline), all apps larger than 1MB except
  // for the screensaver should be deferred.
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(
      kLargeExtensionSize, extension_misc::kScreensaverAppId));
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));

  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOffline);
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(
      kLargeExtensionSize, extension_misc::kScreensaverAppId));
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));

  chromeos::DemoSession::ResetDemoConfigForTesting();
}