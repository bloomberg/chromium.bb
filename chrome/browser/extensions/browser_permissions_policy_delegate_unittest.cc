// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class BrowserPermissionsPolicyDelegateTest : public testing::Test {
 protected:
  virtual void SetUp() {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test");
  }
  virtual void TearDown() {
    // Need to delete profile here before the UI thread is destroyed.
    profile_manager_->DeleteTestingProfile("test");
    profile_manager_.reset();
  }
 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
};

#if !defined(OS_CHROMEOS)
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
#endif

}  // namespace

#if !defined(OS_CHROMEOS)
// Tests that CanExecuteScriptOnPage returns false for the signin process,
// all else being equal.
TEST_F(BrowserPermissionsPolicyDelegateTest, CanExecuteScriptOnPage) {
  GURL kSigninUrl(
      "https://accounts.google.com/ServiceLogin?service=chromiumsync");
  ASSERT_TRUE(SigninManager::IsWebBasedSigninFlowURL(kSigninUrl));

  content::MockRenderProcessHost signin_process(profile_);
  content::MockRenderProcessHost normal_process(profile_);
  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile_);
  ASSERT_TRUE(signin_client);
  signin_client->SetSigninProcess(signin_process.GetID());

  scoped_refptr<const Extension> extension(CreateTestExtension("a"));
  std::string error;

  // The same call should succeed with a normal process, but fail with a signin
  // process.
  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(permissions_data->CanAccessPage(extension.get(),
                                              kSigninUrl,
                                              kSigninUrl,
                                              -1,  // no tab id.
                                              normal_process.GetID(),
                                              &error))
      << error;
  EXPECT_FALSE(permissions_data->CanAccessPage(extension.get(),
                                               kSigninUrl,
                                               kSigninUrl,
                                               -1,  // no tab id.
                                               signin_process.GetID(),
                                               &error))
      << error;
}
#endif

}  // namespace extensions
