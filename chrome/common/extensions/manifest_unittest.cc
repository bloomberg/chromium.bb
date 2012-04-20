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
  string16 error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ(ASCIIToUTF16(""), error);
  AssertType(manifest.get(), Extension::TYPE_EXTENSION);

  // The known key 'background_page' should be accessible.
  std::string value;
  EXPECT_TRUE(manifest->GetString(keys::kBackgroundPageLegacy, &value));
  EXPECT_EQ("bg.html", value);

  // The unknown key 'unknown_key' should be inaccesible.
  value.clear();
  EXPECT_FALSE(manifest->GetString("unknown_key", &value));
  EXPECT_EQ("", value);

  // Set the manifest_version to 2; background_page should stop working.
  value.clear();
  manifest->value()->SetInteger(keys::kManifestVersion, 2);
  EXPECT_FALSE(manifest->GetString("background_page", &value));
  EXPECT_EQ("", value);

  // Validate should also stop working.
  error.clear();
  EXPECT_FALSE(manifest->ValidateManifest(&error));
  {
    Feature feature;
    feature.set_name("background_page");
    feature.set_max_manifest_version(1);
    EXPECT_EQ(ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kFeatureNotAllowed,
        "background_page",
        feature.GetErrorMessage(Feature::INVALID_MAX_MANIFEST_VERSION)), error);
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
  string16 error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ(ASCIIToUTF16(""), error);

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

// Verifies that the various getters filter unknown and restricted keys.
TEST_F(ManifestTest, Getters) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  scoped_ptr<Manifest> manifest(
      new Manifest(Extension::INTERNAL, value.Pass()));
  std::string unknown_key = "asdfaskldjf";

  // Verify that the key filtering works for each of the getters.
  // Get and GetBoolean
  bool expected_bool = true, actual_bool = false;
  manifest->value()->Set(unknown_key, Value::CreateBooleanValue(expected_bool));
  EXPECT_FALSE(manifest->HasKey(unknown_key));
  EXPECT_FALSE(manifest->GetBoolean(unknown_key, &actual_bool));
  EXPECT_FALSE(actual_bool);
  Value* actual_value = NULL;
  EXPECT_FALSE(manifest->Get(unknown_key, &actual_value));
  EXPECT_TRUE(manifest->value()->Remove(unknown_key, NULL));

  // GetInteger
  int expected_int = 5, actual_int = 0;
  manifest->value()->Set(unknown_key, Value::CreateIntegerValue(expected_int));
  EXPECT_FALSE(manifest->GetInteger(unknown_key, &actual_int));
  EXPECT_NE(expected_int, actual_int);
  EXPECT_TRUE(manifest->value()->Remove(unknown_key, NULL));

  // GetString
  std::string expected_str = "hello", actual_str;
  manifest->value()->Set(unknown_key, Value::CreateStringValue(expected_str));
  EXPECT_FALSE(manifest->GetString(unknown_key, &actual_str));
  EXPECT_NE(expected_str, actual_str);
  EXPECT_TRUE(manifest->value()->Remove(unknown_key, NULL));

  // GetString (string16)
  string16 expected_str16(UTF8ToUTF16("hello")), actual_str16;
  manifest->value()->Set(unknown_key, Value::CreateStringValue(expected_str16));
  EXPECT_FALSE(manifest->GetString(unknown_key, &actual_str16));
  EXPECT_NE(expected_str16, actual_str16);
  EXPECT_TRUE(manifest->value()->Remove(unknown_key, NULL));

  // GetDictionary
  DictionaryValue* expected_dict = new DictionaryValue();
  DictionaryValue* actual_dict = NULL;
  expected_dict->Set("foo", Value::CreateStringValue("bar"));
  manifest->value()->Set(unknown_key, expected_dict);
  EXPECT_FALSE(manifest->GetDictionary(unknown_key, &actual_dict));
  EXPECT_EQ(NULL, actual_dict);
  std::string path = unknown_key + ".foo";
  EXPECT_FALSE(manifest->GetString(path, &actual_str));
  EXPECT_NE("bar", actual_str);
  EXPECT_TRUE(manifest->value()->Remove(unknown_key, NULL));

  // GetList
  ListValue* expected_list = new ListValue();
  ListValue* actual_list = NULL;
  expected_list->Append(Value::CreateStringValue("blah"));
  manifest->value()->Set(unknown_key, expected_list);
  EXPECT_FALSE(manifest->GetList(unknown_key, &actual_list));
  EXPECT_EQ(NULL, actual_list);
  EXPECT_TRUE(manifest->value()->Remove(unknown_key, NULL));
}

}  // namespace extensions
