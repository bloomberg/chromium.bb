// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class ManifestTest : public testing::Test {
 public:
  ManifestTest() : default_value_("test") {}

 protected:
  void AssertType(Manifest* manifest, Manifest::Type type) {
    EXPECT_EQ(type, manifest->type());
    EXPECT_EQ(type == Manifest::TYPE_THEME, manifest->is_theme());
    EXPECT_EQ(type == Manifest::TYPE_PLATFORM_APP,
              manifest->is_platform_app());
    EXPECT_EQ(type == Manifest::TYPE_LEGACY_PACKAGED_APP,
              manifest->is_legacy_packaged_app());
    EXPECT_EQ(type == Manifest::TYPE_HOSTED_APP, manifest->is_hosted_app());
    EXPECT_EQ(type == Manifest::TYPE_SHARED_MODULE,
              manifest->is_shared_module());
  }

  // Helper function that replaces the Manifest held by |manifest| with a copy
  // with its |key| changed to |value|. If |value| is NULL, then |key| will
  // instead be deleted.
  void MutateManifest(scoped_ptr<Manifest>* manifest,
                      const std::string& key,
                      base::Value* value) {
    scoped_ptr<base::DictionaryValue> manifest_value(
        manifest->get()->value()->DeepCopy());
    if (value)
      manifest_value->Set(key, value);
    else
      manifest_value->Remove(key, NULL);
    manifest->reset(new Manifest(Manifest::INTERNAL, manifest_value.Pass()));
  }

  std::string default_value_;
};

// Verifies that extensions can access the correct keys.
TEST_F(ManifestTest, Extension) {
  scoped_ptr<base::DictionaryValue> manifest_value(new base::DictionaryValue());
  manifest_value->SetString(keys::kName, "extension");
  manifest_value->SetString(keys::kVersion, "1");
  // Only supported in manifest_version=1.
  manifest_value->SetString(keys::kBackgroundPageLegacy, "bg.html");
  manifest_value->SetString("unknown_key", "foo");

  scoped_ptr<Manifest> manifest(
      new Manifest(Manifest::INTERNAL, manifest_value.Pass()));
  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  ASSERT_EQ(1u, warnings.size());
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

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
      &manifest, keys::kManifestVersion, new base::FundamentalValue(2));
  EXPECT_FALSE(manifest->GetString("background_page", &value));
  EXPECT_EQ("", value);

  // Validate should also give a warning.
  warnings.clear();
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  ASSERT_EQ(2u, warnings.size());
  {
    SimpleFeature feature;
    feature.set_name("background_page");
    feature.set_max_manifest_version(1);
    EXPECT_EQ(
        "'background_page' requires manifest version of 1 or lower.",
        warnings[0].message);
  }

  // Test DeepCopy and Equals.
  scoped_ptr<Manifest> manifest2(manifest->DeepCopy());
  EXPECT_TRUE(manifest->Equals(manifest2.get()));
  EXPECT_TRUE(manifest2->Equals(manifest.get()));
  MutateManifest(
      &manifest, "foo", new base::StringValue("blah"));
  EXPECT_FALSE(manifest->Equals(manifest2.get()));
}

// Verifies that key restriction based on type works.
TEST_F(ManifestTest, ExtensionTypes) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  scoped_ptr<Manifest> manifest(
      new Manifest(Manifest::INTERNAL, value.Pass()));
  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(warnings.empty());

  // By default, the type is Extension.
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

  // Theme.
  MutateManifest(
      &manifest, keys::kTheme, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_THEME);
  MutateManifest(
      &manifest, keys::kTheme, NULL);

  // Shared module.
  MutateManifest(
      &manifest, keys::kExport, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_SHARED_MODULE);
  MutateManifest(
      &manifest, keys::kExport, NULL);

  // Packaged app.
  MutateManifest(
      &manifest, keys::kApp, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_LEGACY_PACKAGED_APP);

  // Platform app with event page.
  MutateManifest(
      &manifest, keys::kPlatformAppBackground, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  MutateManifest(
      &manifest, keys::kPlatformAppBackground, NULL);

  // Platform app with service worker.
  MutateManifest(
      &manifest, keys::kPlatformAppServiceWorker, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  MutateManifest(&manifest, keys::kPlatformAppServiceWorker, NULL);

  // Hosted app.
  MutateManifest(
      &manifest, keys::kWebURLs, new base::ListValue());
  AssertType(manifest.get(), Manifest::TYPE_HOSTED_APP);
  MutateManifest(
      &manifest, keys::kWebURLs, NULL);
  MutateManifest(
      &manifest, keys::kLaunchWebURL, new base::StringValue("foo"));
  AssertType(manifest.get(), Manifest::TYPE_HOSTED_APP);
  MutateManifest(
      &manifest, keys::kLaunchWebURL, NULL);
};

// Verifies that the getters filter restricted keys.
TEST_F(ManifestTest, RestrictedKeys) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  scoped_ptr<Manifest> manifest(
      new Manifest(Manifest::INTERNAL, value.Pass()));
  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(warnings.empty());

  // "Commands" requires manifest version 2.
  const base::Value* output = NULL;
  MutateManifest(
      &manifest, keys::kCommands, new base::DictionaryValue());
  EXPECT_FALSE(manifest->HasKey(keys::kCommands));
  EXPECT_FALSE(manifest->Get(keys::kCommands, &output));

  MutateManifest(
      &manifest, keys::kManifestVersion, new base::FundamentalValue(2));
  EXPECT_TRUE(manifest->HasKey(keys::kCommands));
  EXPECT_TRUE(manifest->Get(keys::kCommands, &output));

  MutateManifest(
      &manifest, keys::kPageAction, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);
  EXPECT_TRUE(manifest->HasKey(keys::kPageAction));
  EXPECT_TRUE(manifest->Get(keys::kPageAction, &output));

  // Platform apps cannot have a "page_action" key.
  MutateManifest(
      &manifest, keys::kPlatformAppBackground, new base::DictionaryValue());
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  EXPECT_FALSE(manifest->HasKey(keys::kPageAction));
  EXPECT_FALSE(manifest->Get(keys::kPageAction, &output));
  MutateManifest(
      &manifest, keys::kPlatformAppBackground, NULL);

  // Platform apps also can't have a "Commands" key.
  EXPECT_FALSE(manifest->HasKey(keys::kCommands));
  EXPECT_FALSE(manifest->Get(keys::kCommands, &output));
};

}  // namespace extensions
