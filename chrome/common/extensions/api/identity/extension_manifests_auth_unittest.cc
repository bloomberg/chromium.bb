// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace keys = extension_manifest_keys;

namespace extensions {

class OAuth2ManifestTest : public ExtensionManifestTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new OAuth2ManifestHandler)->Register();
  }
};

TEST_F(OAuth2ManifestTest, OAuth2SectionParsing) {
  base::DictionaryValue base_manifest;

  base_manifest.SetString(keys::kName, "test");
  base_manifest.SetString(keys::kVersion, "0.1");
  base_manifest.SetInteger(keys::kManifestVersion, 2);
  base_manifest.SetString(keys::kOAuth2ClientId, "client1");
  base::ListValue* scopes = new base::ListValue();
  scopes->Append(new base::StringValue("scope1"));
  scopes->Append(new base::StringValue("scope2"));
  base_manifest.Set(keys::kOAuth2Scopes, scopes);

  // OAuth2 section should be parsed for an extension.
  {
    base::DictionaryValue ext_manifest;
    // Lack of "app" section representa an extension. So the base manifest
    // itself represents an extension.
    ext_manifest.MergeDictionary(&base_manifest);

    Manifest manifest(&ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_EQ("client1", OAuth2Info::GetOAuth2Info(extension).client_id);
    EXPECT_EQ(2U, OAuth2Info::GetOAuth2Info(extension).scopes.size());
    EXPECT_EQ("scope1", OAuth2Info::GetOAuth2Info(extension).scopes[0]);
    EXPECT_EQ("scope2", OAuth2Info::GetOAuth2Info(extension).scopes[1]);
  }

  // OAuth2 section should be parsed for a packaged app.
  {
    base::DictionaryValue app_manifest;
    app_manifest.SetString(keys::kLaunchLocalPath, "launch.html");
    app_manifest.MergeDictionary(&base_manifest);

    Manifest manifest(&app_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_EQ("client1", OAuth2Info::GetOAuth2Info(extension).client_id);
    EXPECT_EQ(2U, OAuth2Info::GetOAuth2Info(extension).scopes.size());
    EXPECT_EQ("scope1", OAuth2Info::GetOAuth2Info(extension).scopes[0]);
    EXPECT_EQ("scope2", OAuth2Info::GetOAuth2Info(extension).scopes[1]);
  }

  // OAuth2 section should NOT be parsed for a hosted app.
  {
    base::DictionaryValue app_manifest;
    app_manifest.SetString(keys::kLaunchWebURL, "http://www.google.com");
    app_manifest.MergeDictionary(&base_manifest);

    Manifest manifest(&app_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_EQ(1U, extension->install_warnings().size());
    const extensions::InstallWarning& warning =
        extension->install_warnings()[0];
    EXPECT_EQ("'oauth2' is only allowed for extensions, legacy packaged apps "
                  "and packaged apps, and this is a hosted app.",
              warning.message);
    EXPECT_EQ("", OAuth2Info::GetOAuth2Info(extension).client_id);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension).scopes.empty());
  }
}

}  // namespace extensions
