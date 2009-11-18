// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_reporter.h"
#include "chrome/common/json_value_serializer.h"
#include "net/base/mime_sniffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;

class ExtensionTest : public testing::Test {
};

// TODO(mad): http://crbug.com/26214
TEST(ExtensionTest, DISABLED_InitFromValueInvalid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  std::string error;
  ExtensionErrorReporter::Init(false);

  // Start with a valid extension manifest
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0")
      .AppendASCII(Extension::kManifestFilename);

  JSONFileValueSerializer serializer(extensions_path);
  scoped_ptr<DictionaryValue> valid_value(
      static_cast<DictionaryValue*>(serializer.Deserialize(&error)));
  EXPECT_EQ("", error);
  ASSERT_TRUE(valid_value.get());
  ASSERT_TRUE(extension.InitFromValue(*valid_value, true, &error));
  ASSERT_EQ("", error);
  EXPECT_EQ("en_US", extension.default_locale());

  scoped_ptr<DictionaryValue> input_value;

  // Test missing and invalid versions
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(keys::kVersion, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidVersion, error);

  input_value->SetInteger(keys::kVersion, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidVersion, error);

  // Test missing and invalid names
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Remove(keys::kName, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidName, error);

  input_value->SetInteger(keys::kName, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidName, error);

  // Test invalid description
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(keys::kDescription, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidDescription, error);

  // Test invalid icons
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(keys::kIcons, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidIcons, error);

  // Test invalid icon paths
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  DictionaryValue* icons = NULL;
  input_value->GetDictionary(keys::kIcons, &icons);
  ASSERT_FALSE(NULL == icons);
  icons->SetInteger(ASCIIToWide(IntToString(128)), 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidIconPath));

  // Test invalid user scripts list
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(keys::kContentScripts, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidContentScriptsList, error);

  // Test invalid user script item
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  ListValue* content_scripts = NULL;
  input_value->GetList(keys::kContentScripts, &content_scripts);
  ASSERT_FALSE(NULL == content_scripts);
  content_scripts->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidContentScript));

  // Test missing and invalid matches array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(keys::kContentScripts, &content_scripts);
  DictionaryValue* user_script = NULL;
  content_scripts->GetDictionary(0, &user_script);
  user_script->Remove(keys::kMatches, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidMatches));

  user_script->Set(keys::kMatches, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidMatches));

  ListValue* matches = new ListValue;
  user_script->Set(keys::kMatches, matches);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidMatchCount));

  // Test invalid match element
  matches->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidMatch));

  // Test missing and invalid files array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(keys::kContentScripts, &content_scripts);
  content_scripts->GetDictionary(0, &user_script);
  user_script->Remove(keys::kJs, NULL);
  user_script->Remove(keys::kCss, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kMissingFile));

  user_script->Set(keys::kJs, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidJsList));

  user_script->Set(keys::kCss, new ListValue);
  user_script->Set(keys::kJs, new ListValue);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kMissingFile));
  user_script->Remove(keys::kCss, NULL);

  ListValue* files = new ListValue;
  user_script->Set(keys::kJs, files);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kMissingFile));

  // Test invalid file element
  files->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidJs));

  user_script->Remove(keys::kJs, NULL);
  // Test the css element
  user_script->Set(keys::kCss, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidCssList));

  // Test invalid file element
  ListValue* css_files = new ListValue;
  user_script->Set(keys::kCss, css_files);
  css_files->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidCss));

  // Test missing and invalid permissions array
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));
  ListValue* permissions = NULL;
  input_value->GetList(keys::kPermissions, &permissions);
  ASSERT_FALSE(NULL == permissions);

  permissions = new ListValue;
  input_value->Set(keys::kPermissions, permissions);
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(0u, ExtensionErrorReporter::GetInstance()->GetErrors()->size());

  input_value->Set(keys::kPermissions, Value::CreateIntegerValue(9));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPermissions));

  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(keys::kPermissions, &permissions);
  permissions->Set(0, Value::CreateIntegerValue(24));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPermission));

  permissions->Set(0, Value::CreateStringValue("www.google.com"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPermission));

  // Test permissions scheme.
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->GetList(keys::kPermissions, &permissions);
  permissions->Set(0, Value::CreateStringValue("file:///C:/foo.txt"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPermissionScheme));

  // Test invalid privacy blacklists list.
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->SetInteger(keys::kPrivacyBlacklists, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidPrivacyBlacklists, error);

  // Test invalid privacy blacklists list item.
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  ListValue* privacy_blacklists = NULL;
  input_value->GetList(keys::kPrivacyBlacklists, &privacy_blacklists);
  ASSERT_FALSE(NULL == privacy_blacklists);
  privacy_blacklists->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPrivacyBlacklistsPath));

  // Test invalid UI surface count (both page action and browser action).
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  DictionaryValue* action = new DictionaryValue;
  action->SetString(keys::kPageActionId, "MyExtensionActionId");
  action->SetString(keys::kName, "MyExtensionActionName");
  ListValue* action_list = new ListValue;
  action_list->Append(action->DeepCopy());
  input_value->Set(keys::kPageActions, action_list);
  input_value->Set(keys::kBrowserAction, action);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_STREQ(error.c_str(), errors::kOneUISurfaceOnly);

  // Test invalid options page url.
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Set(keys::kOptionsPage, Value::CreateNullValue());
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidOptionsPage));

  // Test invalid/empty default locale.
  input_value.reset(static_cast<DictionaryValue*>(valid_value->DeepCopy()));
  input_value->Set(keys::kDefaultLocale, Value::CreateIntegerValue(5));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidDefaultLocale));

  input_value->Set(keys::kDefaultLocale, Value::CreateStringValue(""));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidDefaultLocale));
}

TEST(ExtensionTest, InitFromValueValid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  std::string error;
  DictionaryValue input_value;

  // Test minimal extension
  input_value.SetString(keys::kVersion, "1.0.0.0");
  input_value.SetString(keys::kName, "my extension");

  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  EXPECT_TRUE(Extension::IdIsValid(extension.id()));
  EXPECT_EQ("1.0.0.0", extension.VersionString());
  EXPECT_EQ("my extension", extension.name());
  EXPECT_EQ(extension.id(), extension.url().host());
  EXPECT_EQ(path.value(), extension.path().value());

  // Test with an options page.
  input_value.SetString(keys::kOptionsPage, "options.html");
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  EXPECT_EQ("chrome-extension", extension.options_url().scheme());
  EXPECT_EQ("/options.html", extension.options_url().path());
}

TEST(ExtensionTest, GetResourceURLAndPath) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension extension(path);
  DictionaryValue input_value;
  input_value.SetString(keys::kVersion, "1.0.0.0");
  input_value.SetString(keys::kName, "my extension");
  EXPECT_TRUE(extension.InitFromValue(input_value, false, NULL));

  EXPECT_EQ(extension.url().spec() + "bar/baz.js",
            Extension::GetResourceURL(extension.url(), "bar/baz.js").spec());
  EXPECT_EQ(extension.url().spec() + "baz.js",
            Extension::GetResourceURL(extension.url(), "bar/../baz.js").spec());
  EXPECT_EQ(extension.url().spec() + "baz.js",
            Extension::GetResourceURL(extension.url(), "../baz.js").spec());
}

TEST(ExtensionTest, LoadPageActionHelper) {
#if defined(OS_WIN)
    FilePath path(StringPrintf(L"c:\\extension"));
#else
    FilePath path(StringPrintf("/extension"));
#endif
  Extension extension(path);
  std::string error_msg;
  scoped_ptr<ExtensionAction> action;
  DictionaryValue input;

  // First try with an empty dictionary.
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(action != NULL);
  ASSERT_STREQ("", error_msg.c_str());
  error_msg = "";

  // Now setup some values to use in the action.
  const std::string id("MyExtensionActionId");
  const std::string name("MyExtensionActionName");
  std::string img1("image1.png");
  std::string img2("image2.png");

  // Add the dictionary for the contextual action.
  input.SetString(keys::kPageActionId, id);
  input.SetString(keys::kName, name);
  ListValue* icons = new ListValue;
  icons->Set(0, Value::CreateStringValue(img1));
  icons->Set(1, Value::CreateStringValue(img2));
  input.Set(keys::kPageActionIcons, icons);

  // Parse and read back the values from the object.
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_STREQ("", error_msg.c_str());
  ASSERT_STREQ(id.c_str(), action->id().c_str());
  // No title, so fall back to name.
  ASSERT_STREQ(name.c_str(), action->GetTitle(1).c_str());
  ASSERT_EQ(2u, action->icon_paths()->size());
  ASSERT_STREQ(img1.c_str(), action->icon_paths()->at(0).c_str());
  ASSERT_STREQ(img2.c_str(), action->icon_paths()->at(1).c_str());

  // Explicitly set the same type and parse again.
  input.SetString(keys::kType, values::kPageActionTypeTab);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_STREQ("", error_msg.c_str());

  // Make a deep copy of the input and remove one key at a time and see if we
  // get the right error.
  scoped_ptr<DictionaryValue> copy;

  // First remove id key.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->Remove(keys::kPageActionId, NULL);
  action.reset(extension.LoadExtensionActionHelper(copy.get(), &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_STREQ("", error_msg.c_str());
  error_msg = "";

  // Then remove the name key. It's optional, so no error.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->Remove(keys::kName, NULL);
  action.reset(extension.LoadExtensionActionHelper(copy.get(), &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_STREQ("", action->GetTitle(1).c_str());
  ASSERT_STREQ("", error_msg.c_str());
  error_msg = "";

  // Then remove the icon paths key.
  copy.reset(static_cast<DictionaryValue*>(input.DeepCopy()));
  copy->Remove(keys::kPageActionIcons, NULL);
  action.reset(extension.LoadExtensionActionHelper(copy.get(), &error_msg));
  ASSERT_TRUE(NULL != action.get());
  error_msg = "";

  // Now test that we can parse the new format for page actions.

  // Now setup some values to use in the page action.
  const std::string kTitle("MyExtensionActionTitle");
  const std::string kIcon("image1.png");

  // Add the dictionary for the contextual action.
  input.Clear();
  input.SetString(keys::kPageActionDefaultTitle, kTitle);
  input.SetString(keys::kPageActionDefaultIcon, kIcon);

  // Parse and read back the values from the object.
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(action.get());
  ASSERT_STREQ("", error_msg.c_str());
  ASSERT_EQ(kTitle, action->GetTitle(1));
  ASSERT_EQ(0u, action->icon_paths()->size());

  // Invalid title should give an error even with a valid name.
  input.Clear();
  input.SetInteger(keys::kPageActionDefaultTitle, 42);
  input.SetString(keys::kName, name);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL == action.get());
  ASSERT_STREQ(errors::kInvalidPageActionDefaultTitle, error_msg.c_str());
  error_msg = "";

  // Invalid name should give an error only with no title.
  input.SetString(keys::kPageActionDefaultTitle, kTitle);
  input.SetInteger(keys::kName, 123);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_EQ(kTitle, action->GetTitle(1));
  ASSERT_STREQ("", error_msg.c_str());
  error_msg = "";

  input.Remove(keys::kPageActionDefaultTitle, NULL);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL == action.get());
  ASSERT_STREQ(errors::kInvalidPageActionName, error_msg.c_str());
  error_msg = "";
}

TEST(ExtensionTest, IdIsValid) {
  EXPECT_TRUE(Extension::IdIsValid("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Extension::IdIsValid("pppppppppppppppppppppppppppppppp"));
  EXPECT_TRUE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnop"));
  EXPECT_TRUE(Extension::IdIsValid("ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmno"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnopa"));
  EXPECT_FALSE(Extension::IdIsValid("0123456789abcdef0123456789abcdef"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmnoq"));
  EXPECT_FALSE(Extension::IdIsValid("abcdefghijklmnopabcdefghijklmno0"));
}

TEST(ExtensionTest, GenerateID) {
  const uint8 public_key_info[] = {
    0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
    0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81,
    0x89, 0x02, 0x81, 0x81, 0x00, 0xb8, 0x7f, 0x2b, 0x20, 0xdc, 0x7c, 0x9b,
    0x0c, 0xdc, 0x51, 0x61, 0x99, 0x0d, 0x36, 0x0f, 0xd4, 0x66, 0x88, 0x08,
    0x55, 0x84, 0xd5, 0x3a, 0xbf, 0x2b, 0xa4, 0x64, 0x85, 0x7b, 0x0c, 0x04,
    0x13, 0x3f, 0x8d, 0xf4, 0xbc, 0x38, 0x0d, 0x49, 0xfe, 0x6b, 0xc4, 0x5a,
    0xb0, 0x40, 0x53, 0x3a, 0xd7, 0x66, 0x09, 0x0f, 0x9e, 0x36, 0x74, 0x30,
    0xda, 0x8a, 0x31, 0x4f, 0x1f, 0x14, 0x50, 0xd7, 0xc7, 0x20, 0x94, 0x17,
    0xde, 0x4e, 0xb9, 0x57, 0x5e, 0x7e, 0x0a, 0xe5, 0xb2, 0x65, 0x7a, 0x89,
    0x4e, 0xb6, 0x47, 0xff, 0x1c, 0xbd, 0xb7, 0x38, 0x13, 0xaf, 0x47, 0x85,
    0x84, 0x32, 0x33, 0xf3, 0x17, 0x49, 0xbf, 0xe9, 0x96, 0xd0, 0xd6, 0x14,
    0x6f, 0x13, 0x8d, 0xc5, 0xfc, 0x2c, 0x72, 0xba, 0xac, 0xea, 0x7e, 0x18,
    0x53, 0x56, 0xa6, 0x83, 0xa2, 0xce, 0x93, 0x93, 0xe7, 0x1f, 0x0f, 0xe6,
    0x0f, 0x02, 0x03, 0x01, 0x00, 0x01
  };

  std::string extension_id;
  EXPECT_TRUE(
      Extension::GenerateId(
          std::string(reinterpret_cast<const char*>(&public_key_info[0]),
                      arraysize(public_key_info)),
          &extension_id));
  EXPECT_EQ("melddjfinppjdikinhbgehiennejpfhp", extension_id);
}

TEST(ExtensionTest, UpdateUrls) {
  // Test several valid update urls
  std::vector<std::string> valid;
  valid.push_back("http://test.com");
  valid.push_back("http://test.com/");
  valid.push_back("http://test.com/update");
  valid.push_back("http://test.com/update?check=true");
  for (size_t i = 0; i < valid.size(); i++) {
    GURL url(valid[i]);
    EXPECT_TRUE(url.is_valid());

    DictionaryValue input_value;
#if defined(OS_WIN)
    FilePath path(StringPrintf(L"c:\\extension%i", i));
#else
    FilePath path(StringPrintf("/extension%i", i));
#endif
    Extension extension(path);
    std::string error;

    input_value.SetString(keys::kVersion, "1.0");
    input_value.SetString(keys::kName, "Test");
    input_value.SetString(keys::kUpdateURL, url.spec());

    EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  }

  // Test some invalid update urls
  std::vector<std::string> invalid;
  invalid.push_back("");
  invalid.push_back("test.com");
  valid.push_back("http://test.com/update#whatever");
  for (size_t i = 0; i < invalid.size(); i++) {
    DictionaryValue input_value;
#if defined(OS_WIN)
    FilePath path(StringPrintf(L"c:\\extension%i", i));
#else
    FilePath path(StringPrintf("/extension%i", i));
#endif
    Extension extension(path);
    std::string error;
    input_value.SetString(keys::kVersion, "1.0");
    input_value.SetString(keys::kName, "Test");
    input_value.SetString(keys::kUpdateURL, invalid[i]);

    EXPECT_FALSE(extension.InitFromValue(input_value, false, &error));
    EXPECT_TRUE(MatchPattern(error, errors::kInvalidUpdateURL));
  }
}

// This test ensures that the mimetype sniffing code stays in sync with the
// actual crx files that we test other parts of the system with.
TEST(ExtensionTest, MimeTypeSniffing) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("good.crx");

  std::string data;
  ASSERT_TRUE(file_util::ReadFileToString(path, &data));

  std::string result;
  EXPECT_TRUE(net::SniffMimeType(data.c_str(), data.size(),
              GURL("http://www.example.com/foo.crx"), "", &result));
  EXPECT_EQ(std::string(Extension::kMimeType), result);

  data.clear();
  result.clear();
  path = path.DirName().AppendASCII("bad_magic.crx");
  ASSERT_TRUE(file_util::ReadFileToString(path, &data));
  EXPECT_TRUE(net::SniffMimeType(data.c_str(), data.size(),
              GURL("http://www.example.com/foo.crx"), "", &result));
  EXPECT_EQ("application/octet-stream", result);
}

static Extension* LoadManifest(const std::string& dir,
                               const std::string& test_file) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueSerializer serializer(path);
  scoped_ptr<Value> result(serializer.Deserialize(NULL));
  if (!result.get())
    return NULL;

  std::string error;
  scoped_ptr<Extension> extension(new Extension(path.DirName()));
  extension->InitFromValue(*static_cast<DictionaryValue*>(result.get()),
                           false, &error);

  return extension.release();
}

TEST(ExtensionTest, EffectiveHostPermissions) {
  scoped_ptr<Extension> extension;
  std::set<std::string> hosts;

  extension.reset(LoadManifest("effective_host_permissions", "empty.json"));
  EXPECT_EQ(0u, extension->GetEffectiveHostPermissions().size());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions", "one_host.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_TRUE(hosts.find("www.google.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "one_host_wildcard.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_TRUE(hosts.find("google.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "two_hosts.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(2u, hosts.size());
  EXPECT_TRUE(hosts.find("www.google.com") != hosts.end());
  EXPECT_TRUE(hosts.find("www.reddit.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "duplicate_host.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_TRUE(hosts.find("google.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "https_not_considered.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_TRUE(hosts.find("google.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "two_content_scripts.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(3u, hosts.size());
  EXPECT_TRUE(hosts.find("google.com") != hosts.end());
  EXPECT_TRUE(hosts.find("www.reddit.com") != hosts.end());
  EXPECT_TRUE(hosts.find("news.ycombinator.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "duplicate_content_script.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(2u, hosts.size());
  EXPECT_TRUE(hosts.find("google.com") != hosts.end());
  EXPECT_TRUE(hosts.find("www.reddit.com") != hosts.end());
  EXPECT_FALSE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "all_hosts.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_TRUE(hosts.find("") != hosts.end());
  EXPECT_TRUE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "all_hosts2.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(2u, hosts.size());
  EXPECT_TRUE(hosts.find("") != hosts.end());
  EXPECT_TRUE(hosts.find("www.google.com") != hosts.end());
  EXPECT_TRUE(extension->HasAccessToAllHosts());

  extension.reset(LoadManifest("effective_host_permissions",
                               "all_hosts3.json"));
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_EQ(2u, hosts.size());
  EXPECT_TRUE(hosts.find("") != hosts.end());
  EXPECT_TRUE(hosts.find("www.google.com") != hosts.end());
  EXPECT_TRUE(extension->HasAccessToAllHosts());
}

TEST(ExtensionTest, IsPrivilegeIncrease) {
  const struct {
    const char* base_name;
    bool expect_success;
  } kTests[] = {
    { "allhosts1", false },  // all -> all
    { "allhosts2", false },  // all -> one
    { "allhosts3", true },  // one -> all
    { "hosts1", false },  // http://a,http://b -> http://a,http://b
    { "hosts2", false },  // http://a,http://b -> https://a,http://*.b
    { "hosts3", false },  // http://a,http://b -> http://a
    { "hosts4", true },  // http://a -> http://a,http://b
    { "permissions1", false },  // tabs -> tabs
    { "permissions2", false },  // tabs -> tabs,bookmarks
    { "permissions3", true },  // http://a -> http://a,tabs
    { "permissions4", false },  // plugin -> plugin,tabs
    { "plugin1", false },  // plugin -> plugin
    { "plugin2", false },  // plugin -> none
    { "plugin3", true }  // none -> plugin
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    scoped_ptr<Extension> old_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_old.json"));
    scoped_ptr<Extension> new_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_new.json"));

    EXPECT_EQ(kTests[i].expect_success,
              Extension::IsPrivilegeIncrease(old_extension.get(),
                                             new_extension.get()))
        << kTests[i].base_name;
  }
}
