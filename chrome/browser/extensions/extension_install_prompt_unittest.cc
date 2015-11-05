// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

void VerifyPromptPermissionsCallback(
    const base::Closure& quit_closure,
    size_t regular_permissions_count,
    size_t withheld_permissions_count,
    ExtensionInstallPromptShowParams* params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> install_prompt) {
  ASSERT_TRUE(install_prompt.get());
  EXPECT_EQ(regular_permissions_count,
            install_prompt->GetPermissionCount(
                ExtensionInstallPrompt::REGULAR_PERMISSIONS));
  EXPECT_EQ(withheld_permissions_count,
            install_prompt->GetPermissionCount(
                ExtensionInstallPrompt::WITHHELD_PERMISSIONS));
  quit_closure.Run();
}

class ExtensionInstallPromptUnitTest : public testing::Test {
 public:
  ExtensionInstallPromptUnitTest() {}
  ~ExtensionInstallPromptUnitTest() override {}

  // testing::Test:
  void SetUp() override {
    profile_.reset(new TestingProfile());
  }
  void TearDown() override {
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallPromptUnitTest);
};

}  // namespace

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsPermissionWarnings) {
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermission::kTab);
  scoped_ptr<const PermissionSet> permission_set(
      new PermissionSet(api_permissions, ManifestPermissionSet(),
                        URLPatternSet(), URLPatternSet()));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(
          DictionaryBuilder().Set("name", "foo")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2)
                             .Set("description", "Random Ext")).Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;
  prompt.set_callback_for_test(
      base::Bind(&VerifyPromptPermissionsCallback,
                 run_loop.QuitClosure(),
                 1u,  // |regular_permissions_count|.
                 0u));  // |withheld_permissions_count|.
  prompt.ConfirmPermissions(nullptr,  // no delegate
                            extension.get(), permission_set.Pass());
  run_loop.Run();
}

TEST_F(ExtensionInstallPromptUnitTest, PromptShowsWithheldPermissions) {
  // Enable consent flag so that <all_hosts> permissions get withheld.
  FeatureSwitch::ScopedOverride enable_scripts_switch(
      FeatureSwitch::scripts_require_action(), true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(
          DictionaryBuilder().Set("name", "foo")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2)
                             .Set("description", "Random Ext")
                             .Set("permissions",
                                  ListBuilder().Append("http://*/*")
                                               .Append("http://www.google.com/")
                                               .Append("tabs"))).Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;

  // We expect <all_hosts> to be withheld, but http://www.google.com/ and tabs
  // permissions should be granted as regular permissions.
  prompt.ConfirmInstall(
      nullptr,
      extension.get(),
      base::Bind(&VerifyPromptPermissionsCallback,
                 run_loop.QuitClosure(),
                 2u,  // |regular_permissions_count|.
                 1u));  // |withheld_permissions_count|.
  run_loop.Run();
}

TEST_F(ExtensionInstallPromptUnitTest,
       DelegatedPromptShowsOptionalPermissions) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(
          DictionaryBuilder().Set("name", "foo")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2)
                             .Set("description", "Random Ext")
                             .Set("permissions",
                                  ListBuilder().Append("clipboardRead"))
                             .Set("optional_permissions",
                                  ListBuilder().Append("tabs"))).Build();

  content::TestWebContentsFactory factory;
  ExtensionInstallPrompt prompt(factory.CreateWebContents(profile()));
  base::RunLoop run_loop;
  prompt.set_callback_for_test(
      base::Bind(&VerifyPromptPermissionsCallback,
                 run_loop.QuitClosure(),
                 2u,  // |regular_permissions_count|.
                 0u));  // |withheld_permissions_count|.
  prompt.ConfirmPermissionsForDelegatedInstall(nullptr,  // no delegate
                                               extension.get(),
                                               "Username",
                                               nullptr);  // no icon
  run_loop.Run();
}

}  // namespace extensions
