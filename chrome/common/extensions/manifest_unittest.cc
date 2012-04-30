// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

class ManifestTest : public testing::Test {
 public:
  ManifestTest() : default_value_("test") {}

 protected:
  void AssertType(Manifest* manifest, Extension::Type type) {
    EXPECT_EQ(type, manifest->GetType());
    EXPECT_EQ(type == Extension::TYPE_THEME, manifest->IsTheme());
    EXPECT_EQ(type == Extension::TYPE_PLATFORM_APP, manifest->IsPlatformApp());
    EXPECT_EQ(type == Extension::TYPE_PACKAGED_APP, manifest->IsPackagedApp());
    EXPECT_EQ(type == Extension::TYPE_HOSTED_APP, manifest->IsHostedApp());
  }

  std::string default_value_;
};

// Verifies that extensions can access the correct keys.
TEST_F(ManifestTest, Extension) {
  scoped_ptr<DictionaryValue> manifest_value(new DictionaryValue());
  manifest_value->SetString(keys::kName, "extension");
  manifest_value->SetString(keys::kVersion, "1");
  // Only supported in manifest_version=1.
  manifest_value->SetString(keys::kBackgroundPageLegacy, "bg.html");
  manifest_value->SetString("unknown_key", "foo");

  scoped_ptr<Manifest> manifest(
      new Manifest(Extension::INTERNAL, manifest_value.Pass()));
  std::vector<std::string> warnings;
  manifest->ValidateManifest(&warnings);
  EXPECT_TRUE(warnings.empty());
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);

  // The known key 'background_page' should be accessible.
  std::string value;
  EXPECT_TRUE(manifest->GetString(keys::kBackgroundPageLegacy, &value));
  EXPECT_EQ("bg.html", value);

  // The unknown key 'unknown_key' should be accesible.
  value.clear();
  EXPECT_TRUE(manifest->GetString("unknown_key", &value));
  EXPECT_EQ("foo", value);

  // Set the manifest_version to 2; background_page should stop working.
  value.clear();
  manifest->value()->SetInteger(keys::kManifestVersion, 2);
  EXPECT_FALSE(manifest->GetString("background_page", &value));
  EXPECT_EQ("", value);

  // Validate should also give a warning.
  warnings.clear();
  manifest->ValidateManifest(&warnings);
  ASSERT_EQ(1u, warnings.size());
  {
    Feature feature;
    feature.set_name("background_page");
    feature.set_max_manifest_version(1);
    EXPECT_EQ(feature.GetErrorMessage(Feature::INVALID_MAX_MANIFEST_VERSION),
              warnings[0]);
  }

  // Test DeepCopy and Equals.
  scoped_ptr<Manifest> manifest2(manifest->DeepCopy());
  EXPECT_TRUE(manifest->Equals(manifest2.get()));
  EXPECT_TRUE(manifest2->Equals(manifest.get()));
  manifest->value()->Set("foo", Value::CreateStringValue("blah"));
  EXPECT_FALSE(manifest->Equals(manifest2.get()));
}

// Verifies that key restriction based on type works.
TEST_F(ManifestTest, ExtensionTypes) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  scoped_ptr<Manifest> manifest(
      new Manifest(Extension::INTERNAL, value.Pass()));
  std::vector<std::string> warnings;
  manifest->ValidateManifest(&warnings);
  EXPECT_TRUE(warnings.empty());

  // By default, the type is Extension.
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);

  // Theme.
  manifest->value()->Set(keys::kTheme, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_THEME);
  manifest->value()->Remove(keys::kTheme, NULL);

  // Platform app.
  manifest->value()->Set(keys::kPlatformApp, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);  // must be boolean
  manifest->value()->SetBoolean(keys::kPlatformApp, false);
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);  // must be true
  manifest->value()->SetBoolean(keys::kPlatformApp, true);
  AssertType(manifest.get(), Extension::TYPE_PLATFORM_APP);
  manifest->value()->Remove(keys::kPlatformApp, NULL);

  // Packaged app.
  manifest->value()->Set(keys::kApp, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_PACKAGED_APP);

  // Hosted app.
  manifest->value()->Set(keys::kWebURLs, new ListValue());
  AssertType(manifest.get(), Extension::TYPE_HOSTED_APP);
  manifest->value()->Remove(keys::kWebURLs, NULL);
  manifest->value()->SetString(keys::kLaunchWebURL, "foo");
  AssertType(manifest.get(), Extension::TYPE_HOSTED_APP);
  manifest->value()->Remove(keys::kLaunchWebURL, NULL);
};

// Verifies that the getters filter restricted keys.
TEST_F(ManifestTest, RestrictedKeys) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  scoped_ptr<Manifest> manifest(
      new Manifest(Extension::INTERNAL, value.Pass()));
  std::vector<std::string> warnings;
  manifest->ValidateManifest(&warnings);
  EXPECT_TRUE(warnings.empty());

  // Platform apps cannot have a "page_action" key.
  manifest->value()->Set(keys::kPageAction, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);
  base::Value* output = NULL;
  EXPECT_TRUE(manifest->HasKey(keys::kPageAction));
  EXPECT_TRUE(manifest->Get(keys::kPageAction, &output));

  manifest->value()->SetBoolean(keys::kPlatformApp, true);
  AssertType(manifest.get(), Extension::TYPE_PLATFORM_APP);
  EXPECT_FALSE(manifest->HasKey(keys::kPageAction));
  EXPECT_FALSE(manifest->Get(keys::kPageAction, &output));
  manifest->value()->Remove(keys::kPlatformApp, NULL);

  // "commands" is restricted to manifest_version >= 2.
  manifest->value()->Set(keys::kCommands, new DictionaryValue());
  EXPECT_FALSE(manifest->HasKey(keys::kCommands));
  EXPECT_FALSE(manifest->Get(keys::kCommands, &output));

  manifest->value()->SetInteger(keys::kManifestVersion, 2);
  EXPECT_TRUE(manifest->HasKey(keys::kCommands));
  EXPECT_TRUE(manifest->Get(keys::kCommands, &output));
};

}  // namespace extensions
