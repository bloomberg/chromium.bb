// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

class PluginVmUtilTest : public testing::Test {
 public:
  PluginVmUtilTest() : test_helper_(&testing_profile_) {}

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile testing_profile_;
  PluginVmTestHelper test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmUtilTest);
};

TEST_F(PluginVmUtilTest,
       IsPluginVmAllowedForProfileReturnsTrueOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(&testing_profile_));

  test_helper_.AllowPluginVm();

  EXPECT_TRUE(IsPluginVmAllowedForProfile(&testing_profile_));
}

TEST_F(PluginVmUtilTest,
       IsPluginVmConfiguredReturnsTrueOnceAllConditionsAreMet) {
  EXPECT_FALSE(IsPluginVmConfigured(&testing_profile_));

  testing_profile_.GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);

  EXPECT_TRUE(IsPluginVmConfigured(&testing_profile_));
}

TEST_F(PluginVmUtilTest, GetPluginVmLicenseKey) {
  // If no license key is set, the method should return the empty string.
  EXPECT_EQ(std::string(), GetPluginVmLicenseKey());

  const std::string kLicenseKey = "LICENSE_KEY";
  testing_profile_.ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kLicenseKey);
  EXPECT_EQ(kLicenseKey, GetPluginVmLicenseKey());
}

}  // namespace plugin_vm
