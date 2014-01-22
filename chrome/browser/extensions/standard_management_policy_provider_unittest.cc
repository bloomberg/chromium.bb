// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StandardManagementPolicyProviderTest : public testing::Test {
 public:
  StandardManagementPolicyProviderTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        prefs_(message_loop_.message_loop_proxy().get()),
        provider_(prefs()) {}

 protected:
  ExtensionPrefs* prefs() {
    return prefs_.prefs();
  }

  scoped_refptr<const Extension> CreateExtension(Manifest::Location location,
                                                 bool required) {
    base::DictionaryValue values;
    values.SetString(manifest_keys::kName, "test");
    values.SetString(manifest_keys::kVersion, "0.1");
    std::string error;
    scoped_refptr<const Extension> extension = Extension::Create(
        base::FilePath(), location, values, Extension::NO_FLAGS, &error);
    CHECK(extension.get()) << error;
    return extension;
  }

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestExtensionPrefs prefs_;

  StandardManagementPolicyProvider provider_;
};

// Tests the behavior of the ManagementPolicy provider methods for an
// extension required by policy.
TEST_F(StandardManagementPolicyProviderTest, RequiredExtension) {
  scoped_refptr<const Extension> extension =
      CreateExtension(Manifest::EXTERNAL_POLICY_DOWNLOAD, true);

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
      CreateExtension(Manifest::INTERNAL, false);

  base::string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
  EXPECT_TRUE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
  EXPECT_FALSE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_EQ(base::string16(), error16);
}

}  // namespace extensions
