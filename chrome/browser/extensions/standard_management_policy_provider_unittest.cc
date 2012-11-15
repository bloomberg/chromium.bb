// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StandardManagementPolicyProviderTest : public testing::Test {
 public:
  StandardManagementPolicyProviderTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        prefs_(message_loop_.message_loop_proxy()),
        blacklist_(prefs()),
        provider_(prefs(), &blacklist_) {
  }


 protected:
  ExtensionPrefs* prefs() {
    return prefs_.prefs();
  }

  scoped_refptr<const Extension> CreateExtension(Extension::Location location,
                                                 bool required) {
    base::DictionaryValue values;
    values.SetString(extension_manifest_keys::kName, "test");
    values.SetString(extension_manifest_keys::kVersion, "0.1");
    std::string error;
    scoped_refptr<const Extension> extension = Extension::Create(
        FilePath(), location, values, Extension::NO_FLAGS, &error);
    CHECK(extension.get()) << error;
    return extension;
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestExtensionPrefs prefs_;

  Blacklist blacklist_;

  StandardManagementPolicyProvider provider_;
};

// Tests various areas of blacklist functionality.
TEST_F(StandardManagementPolicyProviderTest, Blacklist) {
  std::string not_installed_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  // Install 5 extensions.
  ExtensionList extensions;
  for (int i = 0; i < 5; i++) {
    std::string name = "test" + base::IntToString(i);
    extensions.push_back(prefs_.AddExtension(name));
  }
  EXPECT_EQ(NULL,
            prefs()->GetInstalledExtensionInfo(not_installed_id).get());

  ExtensionList::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    EXPECT_TRUE(provider_.UserMayLoad(*iter, NULL));
  }
  // Blacklist one installed and one not-installed extension id.
  std::vector<std::string> blacklisted_ids;
  blacklisted_ids.push_back(extensions[0]->id());
  blacklisted_ids.push_back(not_installed_id);
  blacklist_.SetFromUpdater(blacklisted_ids, "version0");

  // Make sure the id we expect to be blacklisted is.
  EXPECT_FALSE(provider_.UserMayLoad(extensions[0], NULL));

  // Make sure the other id's are not blacklisted.
  for (iter = extensions.begin() + 1; iter != extensions.end(); ++iter)
    EXPECT_TRUE(provider_.UserMayLoad(*iter, NULL));

  // Make sure GetInstalledExtensionsInfo returns only the non-blacklisted
  // extensions data.
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> info(
      prefs()->GetInstalledExtensionsInfo());
  EXPECT_EQ(4u, info->size());
  ExtensionPrefs::ExtensionsInfo::iterator info_iter;
  for (info_iter = info->begin(); info_iter != info->end(); ++info_iter) {
    ExtensionInfo* extension_info = info_iter->get();
    EXPECT_NE(extensions[0]->id(), extension_info->extension_id);
  }
}

// Tests the behavior of the ManagementPolicy provider methods for an
// extension required by policy.
TEST_F(StandardManagementPolicyProviderTest, RequiredExtension) {
  scoped_refptr<const Extension> extension =
      CreateExtension(Extension::EXTERNAL_POLICY_DOWNLOAD, true);

  string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(string16(), error16);

  // We won't check the exact wording of the error, but it should say
  // something.
  EXPECT_FALSE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_NE(string16(), error16);
  EXPECT_TRUE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_NE(string16(), error16);
}

// Tests the behavior of the ManagementPolicy provider methods for an
// extension required by policy.
TEST_F(StandardManagementPolicyProviderTest, NotRequiredExtension) {
  scoped_refptr<const Extension> extension =
      CreateExtension(Extension::INTERNAL, false);

  string16 error16;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error16));
  EXPECT_EQ(string16(), error16);
  EXPECT_TRUE(provider_.UserMayModifySettings(extension.get(), &error16));
  EXPECT_EQ(string16(), error16);
  EXPECT_FALSE(provider_.MustRemainEnabled(extension.get(), &error16));
  EXPECT_EQ(string16(), error16);
}

}  // namespace extensions
