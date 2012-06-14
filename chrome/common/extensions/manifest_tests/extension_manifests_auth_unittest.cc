// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace keys = extension_manifest_keys;

TEST_F(ExtensionManifestTest, OAuth2SectionParsing) {
  DictionaryValue base_manifest;

  base_manifest.SetString(keys::kName, "test");
  base_manifest.SetString(keys::kVersion, "0.1");
  base_manifest.SetInteger(keys::kManifestVersion, 2);
  base_manifest.SetString(keys::kOAuth2ClientId, "client1");
  ListValue* scopes = new ListValue();
  scopes->Append(Value::CreateStringValue("scope1"));
  scopes->Append(Value::CreateStringValue("scope2"));
  base_manifest.Set(keys::kOAuth2Scopes, scopes);

  // OAuth2 section should be parsed for an extension.
  {
    DictionaryValue ext_manifest;
    // Lack of "app" section representa an extension. So the base manifest
    // itself represents an extension.
    ext_manifest.MergeDictionary(&base_manifest);

    Manifest manifest(&ext_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_EQ("client1", extension->oauth2_info().client_id);
    EXPECT_EQ(2U, extension->oauth2_info().scopes.size());
    EXPECT_EQ("scope1", extension->oauth2_info().scopes[0]);
    EXPECT_EQ("scope2", extension->oauth2_info().scopes[1]);
  }

  // OAuth2 section should be parsed for a packaged app.
  {
    DictionaryValue app_manifest;
    app_manifest.SetString(keys::kLaunchLocalPath, "launch.html");
    app_manifest.MergeDictionary(&base_manifest);

    Manifest manifest(&app_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_TRUE(extension->install_warnings().empty());
    EXPECT_EQ("client1", extension->oauth2_info().client_id);
    EXPECT_EQ(2U, extension->oauth2_info().scopes.size());
    EXPECT_EQ("scope1", extension->oauth2_info().scopes[0]);
    EXPECT_EQ("scope2", extension->oauth2_info().scopes[1]);
  }

  // OAuth2 section should NOT be parsed for a hosted app.
  {
    DictionaryValue app_manifest;
    app_manifest.SetString(keys::kLaunchWebURL, "http://www.google.com");
    app_manifest.MergeDictionary(&base_manifest);

    Manifest manifest(&app_manifest, "test");
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess(manifest);
    EXPECT_EQ(1U, extension->install_warnings().size());
    const extensions::Extension::InstallWarning& warning =
        extension->install_warnings()[0];
    EXPECT_EQ("'oauth2' is not allowed for specified package type "
              "(theme, app, etc.).",
              warning.message);
    EXPECT_EQ("", extension->oauth2_info().client_id);
    EXPECT_TRUE(extension->oauth2_info().scopes.empty());
  }
}
