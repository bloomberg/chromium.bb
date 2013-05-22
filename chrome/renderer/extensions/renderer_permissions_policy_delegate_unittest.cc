// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class RendererPermissionsPolicyDelegateTest : public testing::Test {
public:
  RendererPermissionsPolicyDelegateTest() {
  }
  virtual void SetUp() {
    policy_delegate_.reset(new RendererPermissionsPolicyDelegate());
  }
protected:
  scoped_ptr<RendererPermissionsPolicyDelegate> policy_delegate_;
};

scoped_refptr<const Extension> CreateTestExtension(const std::string& id) {
  return ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
          .Set("name", "Extension with ID " + id)
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("permissions", ListBuilder().Append("<all_urls>")))
      .SetID(id)
      .Build();
}

}  // namespace

// Tests that CanExecuteScriptOnPage returns false for the signin process,
// all else being equal.
TEST_F(RendererPermissionsPolicyDelegateTest, CanExecuteScriptOnPage) {
  GURL kSigninUrl(
      "https://accounts.google.com/ServiceLogin?service=chromiumsync");
  scoped_refptr<const Extension> extension(CreateTestExtension("a"));
  std::string error;

  EXPECT_TRUE(PermissionsData::CanExecuteScriptOnPage(extension,
                                                      kSigninUrl,
                                                      kSigninUrl,
                                                      -1,
                                                      NULL,
                                                      -1,
                                                      &error)) << error;
  // Pretend we are in the signin process. We should not be able to execute
  // script.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kSigninProcess);
  EXPECT_FALSE(PermissionsData::CanExecuteScriptOnPage(extension,
                                                       kSigninUrl,
                                                       kSigninUrl,
                                                       -1,
                                                       NULL,
                                                       -1,
                                                       &error)) << error;
}

}  // namespace extensions
