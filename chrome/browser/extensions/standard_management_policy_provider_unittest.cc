// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StandardManagementPolicyProviderTest : public testing::Test {
 public:
  StandardManagementPolicyProviderTest()
      : prefs_(base::ThreadTaskRunnerHandle::Get()),
        settings_(new ExtensionManagement(prefs()->pref_service(), false)),
        provider_(settings_.get()) {}

 protected:
  ExtensionPrefs* prefs() {
    return prefs_.prefs();
  }

  scoped_refptr<const Extension> CreateExtension(Manifest::Location location) {
    return ExtensionBuilder("test").SetLocation(location).Build();
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  TestExtensionPrefs prefs_;
  std::unique_ptr<ExtensionManagement> settings_;

  StandardManagementPolicyProvider provider_;
};

// Tests the behavior of the ManagementPolicy provider methods for an
// extension required by policy.
TEST_F(StandardManagementPolicyProviderTest, RequiredExtension) {
  scoped_refptr<const Extension> extension =
      CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD);

  base::string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);

  // We won't check the exact wording of the error, but it should say
  // something.
  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(base::string16(), error16);
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(base::string16(), error16);
}

// Tests the behavior of the ManagementPolicy provider methods for an
// extension required by policy.
TEST_F(StandardManagementPolicyProviderTest, NotRequiredExtension) {
  scoped_refptr<const Extension> extension =
      CreateExtension(Manifest::INTERNAL);

  base::string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
  EXPECT_TRUE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
  EXPECT_FALSE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
}

}  // namespace extensions
