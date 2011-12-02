// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace extensions {

namespace {

// Keys that define types.
const char* kTypeKeys[] = {
  keys::kApp,
  keys::kTheme,
  keys::kPlatformApp
};

// Keys that are not accesible by themes.
const char* kNotThemeKeys[] = {
  keys::kBrowserAction,
  keys::kPageAction,
  keys::kPageActions,
  keys::kChromeURLOverrides,
  keys::kPermissions,
  keys::kOptionalPermissions,
  keys::kOptionsPage,
  keys::kBackground,
  keys::kOfflineEnabled,
  keys::kMinimumChromeVersion,
  keys::kRequirements,
  keys::kConvertedFromUserScript,
  keys::kNaClModules,
  keys::kPlugins,
  keys::kContentScripts,
  keys::kOmnibox,
  keys::kDevToolsPage,
  keys::kSidebar,
  keys::kHomepageURL,
  keys::kContentSecurityPolicy,
  keys::kFileBrowserHandlers,
  keys::kIncognito,
  keys::kInputComponents,
  keys::kTtsEngine,
  keys::kIntents
};

// Keys that are not accessible by hosted apps.
const char* kNotHostedAppKeys[] = {
  keys::kBrowserAction,
  keys::kPageAction,
  keys::kPageActions,
  keys::kChromeURLOverrides,
  keys::kContentScripts,
  keys::kOmnibox,
  keys::kDevToolsPage,
  keys::kSidebar,
  keys::kHomepageURL,
  keys::kContentSecurityPolicy,
  keys::kFileBrowserHandlers,
  keys::kIncognito,
  keys::kInputComponents,
  keys::kTtsEngine,
  keys::kIntents
};

// Keys not accessible by packaged aps.
const char* kNotPackagedAppKeys[] = {
  keys::kBrowserAction,
  keys::kPageAction,
  keys::kPageActions,
  keys::kChromeURLOverrides,
};

// Keys not accessible by platform apps.
const char* kNotPlatformAppKeys[] = {
  keys::kBrowserAction,
  keys::kPageAction,
  keys::kPageActions,
  keys::kChromeURLOverrides,
  keys::kContentScripts,
  keys::kOmnibox,
  keys::kDevToolsPage,
  keys::kSidebar,
  keys::kHomepageURL,
};

// Returns all the manifest keys not including those in |filtered| or kTypeKeys.
std::set<std::string> GetAccessibleKeys(const char* filtered[], size_t length) {
  std::set<std::string> all_keys = Manifest::GetAllKnownKeys();
  std::set<std::string> filtered_keys(filtered, filtered + length);

  // Starting with all possible manfiest keys, remove the keys that aren't
  // accessible for the given type.
  std::set<std::string> intermediate;
  std::set_difference(all_keys.begin(), all_keys.end(),
                      filtered_keys.begin(), filtered_keys.end(),
                      std::insert_iterator<std::set<std::string> >(
                          intermediate, intermediate.begin()));

  // Then remove the keys that specify types (app, platform_app, etc.).
  std::set<std::string> result;
  std::set<std::string> type_keys(
      kTypeKeys, kTypeKeys + ARRAYSIZE_UNSAFE(kTypeKeys));
  std::set_difference(intermediate.begin(), intermediate.end(),
                      type_keys.begin(), type_keys.end(),
                      std::insert_iterator<std::set<std::string> >(
                          result, result.begin()));

  return result;
}

}  // namespace

class ManifestTest : public testing::Test {
 public:
  ManifestTest() : default_value_("test") {}

 protected:
  void AssertType(Manifest* manifest, Manifest::Type type) {
    EXPECT_EQ(type, manifest->GetType());
    EXPECT_EQ(type == Manifest::kTypeTheme, manifest->IsTheme());
    EXPECT_EQ(type == Manifest::kTypePlatformApp, manifest->IsPlatformApp());
    EXPECT_EQ(type == Manifest::kTypePackagedApp, manifest->IsPackagedApp());
    EXPECT_EQ(type == Manifest::kTypeHostedApp, manifest->IsHostedApp());
  }

  void TestRestrictedKeys(Manifest* manifest,
                          const char* restricted_keys[],
                          size_t restricted_keys_length) {
    // Verify that the keys on the restricted key list for the given manifest
    // fail validation and are filtered out.
    DictionaryValue* value = manifest->value();
    for (size_t i = 0; i < restricted_keys_length; ++i) {
      std::string error, str;
      value->Set(restricted_keys[i], Value::CreateStringValue(default_value_));
      EXPECT_FALSE(manifest->ValidateManifest(&error));
      EXPECT_EQ(error, ExtensionErrorUtils::FormatErrorMessage(
          errors::kFeatureNotAllowed, restricted_keys[i]));
      EXPECT_FALSE(manifest->GetString(restricted_keys[i], &str));
      EXPECT_TRUE(value->Remove(restricted_keys[i], NULL));
    }
  }

  std::string default_value_;
};

// Verifies that extensions can access the correct keys.
TEST_F(ManifestTest, Extension) {
  // Generate the list of keys accessible by extensions.
  std::set<std::string> extension_keys = GetAccessibleKeys(NULL, 0u);

  // Construct the underlying value using every single key other than those
  // on the restricted list.. We can use the same value for every key because we
  // validate only by checking the presence of the keys.
  DictionaryValue* value = new DictionaryValue();
  for (std::set<std::string>::iterator i = extension_keys.begin();
       i != extension_keys.end(); ++i)
    value->Set(*i, Value::CreateStringValue(default_value_));

  scoped_ptr<Manifest> manifest(new Manifest(value));
  std::string error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ("", error);
  AssertType(manifest.get(), Manifest::kTypeExtension);

  // Verify that all the extension keys are accessible.
  for (std::set<std::string>::iterator i = extension_keys.begin();
       i != extension_keys.end(); ++i) {
    std::string value;
    manifest->GetString(*i, &value);
    EXPECT_EQ(default_value_, value) << *i;
  }

  // Test DeepCopy and Equals.
  scoped_ptr<Manifest> manifest2(manifest->DeepCopy());
  EXPECT_TRUE(manifest->Equals(manifest2.get()));
  EXPECT_TRUE(manifest2->Equals(manifest.get()));
  value->Set("foo", Value::CreateStringValue("blah"));
  EXPECT_FALSE(manifest->Equals(manifest2.get()));
}

// Verifies that themes can access the right keys.
TEST_F(ManifestTest, Theme) {
  std::set<std::string> theme_keys =
      GetAccessibleKeys(kNotThemeKeys, ARRAYSIZE_UNSAFE(kNotThemeKeys));

  DictionaryValue* value = new DictionaryValue();
  for (std::set<std::string>::iterator i = theme_keys.begin();
       i != theme_keys.end(); ++i)
    value->Set(*i, Value::CreateStringValue(default_value_));

  std::string theme_key = keys::kTheme + std::string(".test");
  value->Set(theme_key, Value::CreateStringValue(default_value_));

  scoped_ptr<Manifest> manifest(new Manifest(value));
  std::string error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ("", error);
  AssertType(manifest.get(), Manifest::kTypeTheme);

  // Verify that all the theme keys are accessible.
  std::string str;
  for (std::set<std::string>::iterator i = theme_keys.begin();
       i != theme_keys.end(); ++i) {
    EXPECT_TRUE(manifest->GetString(*i, &str));
    EXPECT_EQ(default_value_, str) << *i;
  }
  EXPECT_TRUE(manifest->GetString(theme_key, &str));
  EXPECT_EQ(default_value_, str) << theme_key;

  // And that all the other keys fail validation and are filtered out
  TestRestrictedKeys(manifest.get(), kNotThemeKeys,
                     ARRAYSIZE_UNSAFE(kNotThemeKeys));
};

// Verifies that platform apps can access the right keys.
TEST_F(ManifestTest, PlatformApp) {
  std::set<std::string> platform_keys = GetAccessibleKeys(
      kNotPlatformAppKeys,
      ARRAYSIZE_UNSAFE(kNotPlatformAppKeys));

  DictionaryValue* value = new DictionaryValue();
  for (std::set<std::string>::iterator i = platform_keys.begin();
       i != platform_keys.end(); ++i)
    value->Set(*i, Value::CreateStringValue(default_value_));

  value->Set(keys::kPlatformApp, Value::CreateBooleanValue(true));

  scoped_ptr<Manifest> manifest(new Manifest(value));
  std::string error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ("", error);
  AssertType(manifest.get(), Manifest::kTypePlatformApp);

  // Verify that all the platform app keys are accessible.
  std::string str;
  for (std::set<std::string>::iterator i = platform_keys.begin();
       i != platform_keys.end(); ++i) {
    EXPECT_TRUE(manifest->GetString(*i, &str));
    EXPECT_EQ(default_value_, str) << *i;
  }
  bool is_platform_app = false;
  EXPECT_TRUE(manifest->GetBoolean(keys::kPlatformApp, &is_platform_app));
  EXPECT_TRUE(is_platform_app) << keys::kPlatformApp;

  // And that all the other keys fail validation and are filtered out.
  TestRestrictedKeys(manifest.get(), kNotPlatformAppKeys,
                     ARRAYSIZE_UNSAFE(kNotPlatformAppKeys));
};

// Verifies that hosted apps can access the right keys.
TEST_F(ManifestTest, HostedApp) {
  std::set<std::string> keys = GetAccessibleKeys(
      kNotHostedAppKeys,
      ARRAYSIZE_UNSAFE(kNotHostedAppKeys));

  DictionaryValue* value = new DictionaryValue();
  for (std::set<std::string>::iterator i = keys.begin();
       i != keys.end(); ++i)
    value->Set(*i, Value::CreateStringValue(default_value_));

  value->Set(keys::kWebURLs, Value::CreateStringValue(default_value_));

  scoped_ptr<Manifest> manifest(new Manifest(value));
  std::string error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ("", error);
  AssertType(manifest.get(), Manifest::kTypeHostedApp);

  // Verify that all the hosted app keys are accessible.
  std::string str;
  for (std::set<std::string>::iterator i = keys.begin();
       i != keys.end(); ++i) {
    EXPECT_TRUE(manifest->GetString(*i, &str));
    EXPECT_EQ(default_value_, str) << *i;
  }
  EXPECT_TRUE(manifest->GetString(keys::kWebURLs, &str));
  EXPECT_EQ(default_value_, str) << keys::kWebURLs;

  // And that all the other keys fail validation and are filtered out.
  TestRestrictedKeys(manifest.get(), kNotHostedAppKeys,
                     ARRAYSIZE_UNSAFE(kNotHostedAppKeys));
};

// Verifies that packaged apps can access the right keys.
TEST_F(ManifestTest, PackagedApp) {
  std::set<std::string> keys = GetAccessibleKeys(
      kNotPackagedAppKeys,
      ARRAYSIZE_UNSAFE(kNotPackagedAppKeys));

  DictionaryValue* value = new DictionaryValue();
  for (std::set<std::string>::iterator i = keys.begin();
       i != keys.end(); ++i)
    value->Set(*i, Value::CreateStringValue(default_value_));
  value->Set(keys::kApp, Value::CreateStringValue(default_value_));

  scoped_ptr<Manifest> manifest(new Manifest(value));
  std::string error;
  EXPECT_TRUE(manifest->ValidateManifest(&error));
  EXPECT_EQ("", error);
  AssertType(manifest.get(), Manifest::kTypePackagedApp);

  // Verify that all the packaged app keys are accessible.
  std::string str;
  for (std::set<std::string>::iterator i = keys.begin();
       i != keys.end(); ++i) {
    EXPECT_TRUE(manifest->GetString(*i, &str));
    EXPECT_EQ(default_value_, str) << *i;
  }
  EXPECT_TRUE(manifest->GetString(keys::kApp, &str));
  EXPECT_EQ(default_value_, str) << keys::kApp;

  // And that all the other keys fail validation and are filtered out.
  TestRestrictedKeys(manifest.get(), kNotPackagedAppKeys,
                     ARRAYSIZE_UNSAFE(kNotPackagedAppKeys));
};

// Verifies that the various getters filter unknown and restricted keys.
TEST_F(ManifestTest, Getters) {
  DictionaryValue* value = new DictionaryValue();
  scoped_ptr<Manifest> manifest(new Manifest(value));
  std::string unknown_key = "asdfaskldjf";

  // Verify that the key filtering works for each of the getters.
  // Get and GetBoolean
  bool expected_bool = true, actual_bool = false;
  value->Set(unknown_key, Value::CreateBooleanValue(expected_bool));
  EXPECT_FALSE(manifest->HasKey(unknown_key));
  EXPECT_FALSE(manifest->GetBoolean(unknown_key, &actual_bool));
  EXPECT_FALSE(actual_bool);
  Value* actual_value = NULL;
  EXPECT_FALSE(manifest->Get(unknown_key, &actual_value));
  EXPECT_TRUE(value->Remove(unknown_key, NULL));

  // GetInteger
  int expected_int = 5, actual_int = 0;
  value->Set(unknown_key, Value::CreateIntegerValue(expected_int));
  EXPECT_FALSE(manifest->GetInteger(unknown_key, &actual_int));
  EXPECT_NE(expected_int, actual_int);
  EXPECT_TRUE(value->Remove(unknown_key, NULL));

  // GetString
  std::string expected_str = "hello", actual_str;
  value->Set(unknown_key, Value::CreateStringValue(expected_str));
  EXPECT_FALSE(manifest->GetString(unknown_key, &actual_str));
  EXPECT_NE(expected_str, actual_str);
  EXPECT_TRUE(value->Remove(unknown_key, NULL));

  // GetString (string16)
  string16 expected_str16(UTF8ToUTF16("hello")), actual_str16;
  value->Set(unknown_key, Value::CreateStringValue(expected_str16));
  EXPECT_FALSE(manifest->GetString(unknown_key, &actual_str16));
  EXPECT_NE(expected_str16, actual_str16);
  EXPECT_TRUE(value->Remove(unknown_key, NULL));

  // GetDictionary
  DictionaryValue* expected_dict = new DictionaryValue();
  DictionaryValue* actual_dict = NULL;
  expected_dict->Set("foo", Value::CreateStringValue("bar"));
  value->Set(unknown_key, expected_dict);
  EXPECT_FALSE(manifest->GetDictionary(unknown_key, &actual_dict));
  EXPECT_EQ(NULL, actual_dict);
  std::string path = unknown_key + ".foo";
  EXPECT_FALSE(manifest->GetString(path, &actual_str));
  EXPECT_NE("bar", actual_str);
  EXPECT_TRUE(value->Remove(unknown_key, NULL));

  // GetList
  ListValue* expected_list = new ListValue();
  ListValue* actual_list = NULL;
  expected_list->Append(Value::CreateStringValue("blah"));
  value->Set(unknown_key, expected_list);
  EXPECT_FALSE(manifest->GetList(unknown_key, &actual_list));
  EXPECT_EQ(NULL, actual_list);
  EXPECT_TRUE(value->Remove(unknown_key, NULL));
}

}  // namespace extensions
