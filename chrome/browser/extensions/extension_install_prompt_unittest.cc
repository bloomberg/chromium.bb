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
    const ExtensionInstallPrompt::ShowParams& params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> install_prompt) {
  ASSERT_TRUE(install_prompt.get());
  EXPECT_EQ(1u, install_prompt->GetPermissionCount());
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
  prompt.set_callback_for_test(base::Bind(&VerifyPromptPermissionsCallback,
                                          run_loop.QuitClosure()));
  prompt.ConfirmPermissions(NULL,  // no delegate
                            extension,
                            permission_set);
  run_loop.Run();
}

}  // namespace extensions
