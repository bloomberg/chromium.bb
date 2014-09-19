// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "content/public/test/test_browser_thread_bundle.h"
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

void VerifyPromptPermissionsCallback(
    const base::Closure& quit_closure,
    size_t regular_permissions_count,
    size_t withheld_permissions_count,
    const ExtensionInstallPrompt::ShowParams& params,
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

TEST(ExtensionInstallPromptUnittest, PromptShowsPermissionWarnings) {
  content::TestBrowserThreadBundle thread_bundle;
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermission::kTab);
  scoped_refptr<PermissionSet> permission_set =
      new PermissionSet(api_permissions,
                        ManifestPermissionSet(),
                        URLPatternSet(),
                        URLPatternSet());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(
          DictionaryBuilder().Set("name", "foo")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2)
                             .Set("description", "Random Ext")).Build();
  ExtensionInstallPrompt prompt(NULL /* no web contents in this test */);
  base::RunLoop run_loop;
  prompt.set_callback_for_test(
      base::Bind(&VerifyPromptPermissionsCallback,
                 run_loop.QuitClosure(),
                 1u,  // |regular_permissions_count|.
                 0u));  // |withheld_permissions_count|.
  prompt.ConfirmPermissions(NULL,  // no delegate
                            extension.get(),
                            permission_set.get());
  run_loop.Run();
}

TEST(ExtensionInstallPromptUnittest, PromptShowsWithheldPermissions) {
  content::TestBrowserThreadBundle thread_bundle;

  // Enable consent flag so that <all_hosts> permissions get withheld.
  FeatureSwitch::ScopedOverride enable_scripts_switch(
      FeatureSwitch::scripts_require_action(), true);

  ListBuilder permissions;
  permissions.Append("http://*/*");
  permissions.Append("http://www.google.com/");
  permissions.Append("tabs");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder().SetManifest(
          DictionaryBuilder().Set("name", "foo")
                             .Set("version", "1.0")
                             .Set("manifest_version", 2)
                             .Set("description", "Random Ext")
                             .Set("permissions", permissions)).Build();
  ExtensionInstallPrompt prompt(NULL /* no web contents in this test */);
  base::RunLoop run_loop;

  // We expect <all_hosts> to be withheld, but http://www.google.com/ and tabs
  // permissions should be granted as regular permissions.
  prompt.ConfirmInstall(
      NULL,
      extension.get(),
      base::Bind(&VerifyPromptPermissionsCallback,
                 run_loop.QuitClosure(),
                 2u,  // |regular_permissions_count|.
                 1u));  // |withheld_permissions_count|.
  run_loop.Run();
}

}  // namespace extensions
