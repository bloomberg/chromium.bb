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
    EXPECT_EQ(type, manifest->type());
    EXPECT_EQ(type == Extension::TYPE_THEME, manifest->is_theme());
    EXPECT_EQ(type == Extension::TYPE_PLATFORM_APP,
              manifest->is_platform_app());
    EXPECT_EQ(type == Extension::TYPE_PACKAGED_APP,
              manifest->is_packaged_app());
    EXPECT_EQ(type == Extension::TYPE_HOSTED_APP, manifest->is_hosted_app());
  }

  // Helper function that replaces the Manifest held by |manifest| with a copy
  // with its |key| changed to |value|. If |value| is NULL, then |key| will
  // instead be deleted.
  void MutateManifest(
      scoped_ptr<Manifest>* manifest, const std::string& key, Value* value) {
    scoped_ptr<DictionaryValue> manifest_value(
        manifest->get()->value()->DeepCopy());
    if (value)
      manifest_value->Set(key, value);
    else
      manifest_value->Remove(key, NULL);
    manifest->reset(new Manifest(Extension::INTERNAL, manifest_value.Pass()));
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
  std::string error;
  std::vector<std::string> warnings;
  manifest->ValidateManifest(&error, &warnings);
  EXPECT_TRUE(error.empty());
  ASSERT_EQ(1u, warnings.size());
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
  MutateManifest(
      &manifest, keys::kManifestVersion, Value::CreateIntegerValue(2));
  EXPECT_FALSE(manifest->GetString("background_page", &value));
  EXPECT_EQ("", value);

  // Validate should also give a warning.
  warnings.clear();
  manifest->ValidateManifest(&error, &warnings);
  EXPECT_TRUE(error.empty());
  ASSERT_EQ(2u, warnings.size());
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
  MutateManifest(
      &manifest, "foo", Value::CreateStringValue("blah"));
  EXPECT_FALSE(manifest->Equals(manifest2.get()));
}

// Verifies that key restriction based on type works.
TEST_F(ManifestTest, ExtensionTypes) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  scoped_ptr<Manifest> manifest(
      new Manifest(Extension::INTERNAL, value.Pass()));
  std::string error;
  std::vector<std::string> warnings;
  manifest->ValidateManifest(&error, &warnings);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(warnings.empty());

  // By default, the type is Extension.
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);

  // Theme.
  MutateManifest(
      &manifest, keys::kTheme, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_THEME);
  MutateManifest(
      &manifest, keys::kTheme, NULL);

  // Platform app.
  MutateManifest(
      &manifest, keys::kPlatformApp, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);  // must be boolean
  MutateManifest(
      &manifest, keys::kPlatformApp, Value::CreateBooleanValue(false));
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);  // must be true
  MutateManifest(
      &manifest, keys::kPlatformApp, Value::CreateBooleanValue(true));
  AssertType(manifest.get(), Extension::TYPE_PLATFORM_APP);
  MutateManifest(
      &manifest, keys::kPlatformApp, NULL);

  // Packaged app.
  MutateManifest(
      &manifest, keys::kApp, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_PACKAGED_APP);

  // Hosted app.
  MutateManifest(
      &manifest, keys::kWebURLs, new ListValue());
  AssertType(manifest.get(), Extension::TYPE_HOSTED_APP);
  MutateManifest(
      &manifest, keys::kWebURLs, NULL);
  MutateManifest(
      &manifest, keys::kLaunchWebURL, Value::CreateStringValue("foo"));
  AssertType(manifest.get(), Extension::TYPE_HOSTED_APP);
  MutateManifest(
      &manifest, keys::kLaunchWebURL, NULL);
};

// Verifies that the getters filter restricted keys.
TEST_F(ManifestTest, RestrictedKeys) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  scoped_ptr<Manifest> manifest(
      new Manifest(Extension::INTERNAL, value.Pass()));
  std::string error;
  std::vector<std::string> warnings;
  manifest->ValidateManifest(&error, &warnings);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(warnings.empty());

  // Platform apps cannot have a "page_action" key.
  MutateManifest(
      &manifest, keys::kPageAction, new DictionaryValue());
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);
  base::Value* output = NULL;
  EXPECT_TRUE(manifest->HasKey(keys::kPageAction));
  EXPECT_TRUE(manifest->Get(keys::kPageAction, &output));

  MutateManifest(
      &manifest, keys::kPlatformApp, Value::CreateBooleanValue(true));
  AssertType(manifest.get(), Extension::TYPE_PLATFORM_APP);
  EXPECT_FALSE(manifest->HasKey(keys::kPageAction));
  EXPECT_FALSE(manifest->Get(keys::kPageAction, &output));
  MutateManifest(
      &manifest, keys::kPlatformApp, NULL);

  // "commands" is restricted to manifest_version >= 2.
  MutateManifest(
      &manifest, keys::kCommands, new DictionaryValue());
  EXPECT_FALSE(manifest->HasKey(keys::kCommands));
  EXPECT_FALSE(manifest->Get(keys::kCommands, &output));

  MutateManifest(
      &manifest, keys::kManifestVersion, Value::CreateIntegerValue(2));
  EXPECT_TRUE(manifest->HasKey(keys::kCommands));
  EXPECT_TRUE(manifest->Get(keys::kCommands, &output));
};

}  // namespace extensions
