// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

namespace extensions {

class OAuth2ManifestTest : public ExtensionManifestTest {
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
  base_manifest.SetBoolean(keys::kOAuth2AutoApprove, true);

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
    EXPECT_EQ("client1", OAuth2Info::GetOAuth2Info(extension.get()).client_id);
    EXPECT_EQ(2U, OAuth2Info::GetOAuth2Info(extension.get()).scopes.size());
    EXPECT_EQ("scope1", OAuth2Info::GetOAuth2Info(extension.get()).scopes[0]);
    EXPECT_EQ("scope2", OAuth2Info::GetOAuth2Info(extension.get()).scopes[1]);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
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
    EXPECT_EQ("client1", OAuth2Info::GetOAuth2Info(extension.get()).client_id);
    EXPECT_EQ(2U, OAuth2Info::GetOAuth2Info(extension.get()).scopes.size());
    EXPECT_EQ("scope1", OAuth2Info::GetOAuth2Info(extension.get()).scopes[0]);
    EXPECT_EQ("scope2", OAuth2Info::GetOAuth2Info(extension.get()).scopes[1]);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
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
    EXPECT_EQ("", OAuth2Info::GetOAuth2Info(extension.get()).client_id);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).scopes.empty());
    EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
  }
}

TEST_F(OAuth2ManifestTest, OAuth2AutoApprove) {
  base::DictionaryValue* base_manifest;
  scoped_ptr<Value> parsed_manifest = base::test::ParseJson(
      "{ \n"
      "  \"name\": \"test\", \n"
      "  \"version\": \"0.1\", \n"
      "  \"manifest_version\": 2, \n"
      "  \"oauth2\": { \n"
      "    \"client_id\": \"client1\", \n"
      "    \"scopes\": [ \"scope1\" ], \n"
      "  }, \n"
      "} \n");

  EXPECT_TRUE(parsed_manifest->GetAsDictionary(&base_manifest));

  // OAuth2 section without auto_approve should end up defaulting to false.
  {
    base::DictionaryValue ext_manifest;
    ext_manifest.MergeDictionary(base_manifest);

    Manifest manifest(&ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
  }

  {
    base::DictionaryValue ext_manifest;
    ext_manifest.MergeDictionary(base_manifest);
    // OAuth2 section with auto_approve = false.
    ext_manifest.SetBoolean(keys::kOAuth2AutoApprove, false);

    Manifest manifest(&ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
  }

  {
    base::DictionaryValue ext_manifest;
    ext_manifest.MergeDictionary(base_manifest);
    // OAuth2 section with auto_approve = true.
    ext_manifest.SetBoolean(keys::kOAuth2AutoApprove, true);

    Manifest manifest(&ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
  }
}

}  // namespace extensions
