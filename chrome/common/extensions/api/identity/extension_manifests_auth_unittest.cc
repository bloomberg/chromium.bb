// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/values_test_util.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

// Produces extension ID = "mdbihdcgjmagbcapkhhkjbbdlkflmbfo".
const char kExtensionKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCV9PlZjcTIXfnlB3HXo50OlM/CnIq0y7jm"
    "KfPVyStaWsmFB7NaVnqUXoGb9swBDfVnZ6BrupwnxL76TWEJPo+KQMJ6uz0PPdJWi2jQfZiG"
    "iheDiKH5Gv+dVd67qf7ly8QWW0o8qmFpqBZQpksm1hOGbfsupv9W4c42tMEIicDMLQIDAQAB";
const char kAutoApproveNotAllowedWarning[] =
    "'oauth2.auto_approve' is not allowed for specified extension ID.";

}  // namespace

class OAuth2ManifestTest : public ExtensionManifestTest {
 protected:
  enum AutoApproveValue {
    AUTO_APPROVE_NOT_SET,
    AUTO_APPROVE_FALSE,
    AUTO_APPROVE_TRUE,
    AUTO_APPROVE_INVALID
  };

  enum ClientIdValue {
    CLIENT_ID_DEFAULT,
    CLIENT_ID_NOT_SET,
    CLIENT_ID_EMPTY
  };

  base::DictionaryValue* CreateManifest(
      AutoApproveValue auto_approve,
      bool extension_id_whitelisted,
      ClientIdValue client_id) {
    parsed_manifest_.reset(base::test::ParseJson(
        "{ \n"
        "  \"name\": \"test\", \n"
        "  \"version\": \"0.1\", \n"
        "  \"manifest_version\": 2, \n"
        "  \"oauth2\": { \n"
        "    \"scopes\": [ \"scope1\" ], \n"
        "  }, \n"
        "} \n").release());
    base::DictionaryValue* ext_manifest;
    EXPECT_TRUE(parsed_manifest_->GetAsDictionary(&ext_manifest));
    switch (auto_approve) {
      case AUTO_APPROVE_NOT_SET:
        break;
      case AUTO_APPROVE_FALSE:
        ext_manifest->SetBoolean(keys::kOAuth2AutoApprove, false);
        break;
      case AUTO_APPROVE_TRUE:
        ext_manifest->SetBoolean(keys::kOAuth2AutoApprove, true);
        break;
      case AUTO_APPROVE_INVALID:
        ext_manifest->SetString(keys::kOAuth2AutoApprove, "incorrect value");
        break;
    }
    switch (client_id) {
      case CLIENT_ID_DEFAULT:
        ext_manifest->SetString(keys::kOAuth2ClientId, "client1");
        break;
      case CLIENT_ID_NOT_SET:
        break;
      case CLIENT_ID_EMPTY:
        ext_manifest->SetString(keys::kOAuth2ClientId, "");
    }
    if (extension_id_whitelisted)
      ext_manifest->SetString(keys::kKey, kExtensionKey);
    return ext_manifest;
  }

 private:
  scoped_ptr<base::Value> parsed_manifest_;
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
    ext_manifest.SetString(keys::kKey, kExtensionKey);
    ext_manifest.SetBoolean(keys::kOAuth2AutoApprove, true);

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
    EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
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
    EXPECT_EQ("'oauth2' is only allowed for extensions, legacy packaged apps, "
                  "and packaged apps, but this is a hosted app.",
              warning.message);
    EXPECT_EQ("", OAuth2Info::GetOAuth2Info(extension.get()).client_id);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).scopes.empty());
    EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
  }
}

TEST_F(OAuth2ManifestTest, AutoApproveNotSetExtensionNotOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveFalseExtensionNotOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_FALSE, false, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_EQ(1U, extension->install_warnings().size());
  const extensions::InstallWarning& warning =
      extension->install_warnings()[0];
  EXPECT_EQ(kAutoApproveNotAllowedWarning, warning.message);
  EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveTrueExtensionNotOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_TRUE, false, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_EQ(1U, extension->install_warnings().size());
  const extensions::InstallWarning& warning =
      extension->install_warnings()[0];
  EXPECT_EQ(kAutoApproveNotAllowedWarning, warning.message);
  EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveInvalidExtensionNotOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_INVALID, false, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_EQ(1U, extension->install_warnings().size());
  const extensions::InstallWarning& warning =
      extension->install_warnings()[0];
  EXPECT_EQ(kAutoApproveNotAllowedWarning, warning.message);
  EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveNotSetExtensionOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_NOT_SET, true, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveFalseExtensionOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_FALSE, true, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_FALSE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveTrueExtensionOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).auto_approve);
}

TEST_F(OAuth2ManifestTest, AutoApproveInvalidExtensionOnWhitelist) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_INVALID, true, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  std::string error;
  scoped_refptr<extensions::Extension> extension =
      LoadExtension(manifest, &error);
  EXPECT_EQ(
      "Invalid value for 'oauth2.auto_approve'. Value must be true or false.",
      error);
}

TEST_F(OAuth2ManifestTest, InvalidClientId) {
  {
    base::DictionaryValue* ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_NOT_SET);
    Manifest manifest(ext_manifest, "test");
    std::string error;
    LoadAndExpectError(manifest, errors::kInvalidOAuth2ClientId);
  }

  {
    base::DictionaryValue* ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_EMPTY);
    Manifest manifest(ext_manifest, "test");
    std::string error;
    LoadAndExpectError(manifest, errors::kInvalidOAuth2ClientId);
  }
}

TEST_F(OAuth2ManifestTest, ComponentInvalidClientId) {
  // Component Apps without auto_approve must include a client ID.
  {
    base::DictionaryValue* ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_NOT_SET);
    Manifest manifest(ext_manifest, "test");
    std::string error;
    LoadAndExpectError(manifest,
                       errors::kInvalidOAuth2ClientId,
                       extensions::Manifest::COMPONENT);
  }

  {
    base::DictionaryValue* ext_manifest =
        CreateManifest(AUTO_APPROVE_NOT_SET, false, CLIENT_ID_EMPTY);
    Manifest manifest(ext_manifest, "test");
    std::string error;
    LoadAndExpectError(manifest,
                       errors::kInvalidOAuth2ClientId,
                       extensions::Manifest::COMPONENT);
  }
}

TEST_F(OAuth2ManifestTest, ComponentWithChromeClientId) {
  {
    base::DictionaryValue* ext_manifest =
        CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_NOT_SET);
    Manifest manifest(ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest, extensions::Manifest::COMPONENT);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).client_id.empty());

  }

  {
    base::DictionaryValue* ext_manifest =
        CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_EMPTY);
    Manifest manifest(ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest, extensions::Manifest::COMPONENT);
    EXPECT_TRUE(OAuth2Info::GetOAuth2Info(extension.get()).client_id.empty());

  }
}

TEST_F(OAuth2ManifestTest, ComponentWithStandardClientId) {
  base::DictionaryValue* ext_manifest =
      CreateManifest(AUTO_APPROVE_TRUE, true, CLIENT_ID_DEFAULT);
  Manifest manifest(ext_manifest, "test");
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess(manifest, extensions::Manifest::COMPONENT);
  EXPECT_EQ("client1", OAuth2Info::GetOAuth2Info(extension.get()).client_id);
}

}  // namespace extensions
