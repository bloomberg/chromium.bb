// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/renderer_permissions_policy_delegate.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/mock_render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class RendererPermissionsPolicyDelegateTest : public testing::Test {
 public:
  RendererPermissionsPolicyDelegateTest() {
  }
  virtual void SetUp() {
    testing::Test::SetUp();
    render_thread_.reset(new content::MockRenderThread());
    extension_dispatcher_.reset(new Dispatcher());
    policy_delegate_.reset(
        new RendererPermissionsPolicyDelegate(extension_dispatcher_.get()));
  }
 protected:
  scoped_ptr<Dispatcher> extension_dispatcher_;
  scoped_ptr<RendererPermissionsPolicyDelegate> policy_delegate_;
  scoped_ptr<content::MockRenderThread> render_thread_;
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
TEST_F(RendererPermissionsPolicyDelegateTest, CannotScriptSigninProcess) {
  GURL kSigninUrl(
      "https://accounts.google.com/ServiceLogin?service=chromiumsync");
  scoped_refptr<const Extension> extension(CreateTestExtension("a"));
  std::string error;

  EXPECT_TRUE(PermissionsData::CanExecuteScriptOnPage(
      extension.get(), kSigninUrl, kSigninUrl, -1, NULL, -1, &error)) << error;
  // Pretend we are in the signin process. We should not be able to execute
  // script.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kSigninProcess);
  EXPECT_FALSE(PermissionsData::CanExecuteScriptOnPage(
      extension.get(), kSigninUrl, kSigninUrl, -1, NULL, -1, &error)) << error;
}

// Tests that CanExecuteScriptOnPage returns false for the any process
// which hosts the webstore.
TEST_F(RendererPermissionsPolicyDelegateTest, CannotScriptWebstore) {
  GURL kAnyUrl("http://example.com/");
  scoped_refptr<const Extension> extension(CreateTestExtension("a"));
  std::string error;

  EXPECT_TRUE(PermissionsData::CanExecuteScriptOnPage(
      extension.get(), kAnyUrl, kAnyUrl, -1, NULL, -1, &error)) << error;

  // Pretend we are in the webstore process. We should not be able to execute
  // script.
  scoped_refptr<const Extension> webstore_extension(
      CreateTestExtension(extension_misc::kWebStoreAppId));
  extension_dispatcher_->OnLoadedInternal(webstore_extension);
  extension_dispatcher_->OnActivateExtension(extension_misc::kWebStoreAppId);
  EXPECT_FALSE(PermissionsData::CanExecuteScriptOnPage(
      extension.get(), kAnyUrl, kAnyUrl, -1, NULL, -1, &error)) << error;
}

}  // namespace extensions
