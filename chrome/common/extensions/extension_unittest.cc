// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "base/format_macros.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_sniffer.h"
#include "skia/ext/image_operations.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;

namespace {

void CompareLists(const std::vector<std::string>& expected,
                  const std::vector<std::string>& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

static void AddPattern(ExtensionExtent* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}

class ExtensionTest : public testing::Test {
};

// We persist location values in the preferences, so this is a sanity test that
// someone doesn't accidentally change them.
TEST(ExtensionTest, LocationValuesTest) {
  ASSERT_EQ(0, Extension::INVALID);
  ASSERT_EQ(1, Extension::INTERNAL);
  ASSERT_EQ(2, Extension::EXTERNAL_PREF);
  ASSERT_EQ(3, Extension::EXTERNAL_REGISTRY);
  ASSERT_EQ(4, Extension::LOAD);
  ASSERT_EQ(5, Extension::COMPONENT);
}


// Please don't put any more manifest tests here!!
// Move them to extension_manifest_unittest.cc instead and make them use the
// more data-driven style there instead.
// Bug: http://crbug.com/38462


TEST(ExtensionTest, InitFromValueInvalid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  scoped_refptr<Extension> extension_ptr(new Extension(path,
                                                       Extension::INVALID));
  Extension& extension = *extension_ptr;
  int error_code = 0;
  std::string error;

  // Start with a valid extension manifest
  FilePath extensions_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extensions_path));
  extensions_path = extensions_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0")
      .Append(Extension::kManifestFilename);

  JSONFileValueSerializer serializer(extensions_path);
  scoped_ptr<DictionaryValue> valid_value(
      static_cast<DictionaryValue*>(serializer.Deserialize(&error_code,
                                                           &error)));
  EXPECT_EQ("", error);
  EXPECT_EQ(0, error_code);
  ASSERT_TRUE(valid_value.get());
  ASSERT_TRUE(extension.InitFromValue(*valid_value, true, &error));
  ASSERT_EQ("", error);
  EXPECT_EQ("en_US", extension.default_locale());

  scoped_ptr<DictionaryValue> input_value;

  // Test missing and invalid versions
  input_value.reset(valid_value->DeepCopy());
  input_value->Remove(keys::kVersion, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidVersion, error);

  input_value->SetInteger(keys::kVersion, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidVersion, error);

  // Test missing and invalid names.
  input_value.reset(valid_value->DeepCopy());
  input_value->Remove(keys::kName, NULL);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidName, error);

  input_value->SetInteger(keys::kName, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidName, error);

  // Test invalid description
  input_value.reset(valid_value->DeepCopy());
  input_value->SetInteger(keys::kDescription, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidDescription, error);

  // Test invalid icons
  input_value.reset(valid_value->DeepCopy());
  input_value->SetInteger(keys::kIcons, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidIcons, error);

  // Test invalid icon paths
  input_value.reset(valid_value->DeepCopy());
  DictionaryValue* icons = NULL;
  input_value->GetDictionary(keys::kIcons, &icons);
  ASSERT_FALSE(NULL == icons);
  icons->SetInteger(base::IntToString(128), 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidIconPath));

  // Test invalid user scripts list
  input_value.reset(valid_value->DeepCopy());
  input_value->SetInteger(keys::kContentScripts, 42);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_EQ(errors::kInvalidContentScriptsList, error);

  // Test invalid user script item
  input_value.reset(valid_value->DeepCopy());
  ListValue* content_scripts = NULL;
  input_value->GetList(keys::kContentScripts, &content_scripts);
  ASSERT_FALSE(NULL == content_scripts);
  content_scripts->Set(0, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidContentScript));

  // Test missing and invalid matches array
  input_value.reset(valid_value->DeepCopy());
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

  matches->Set(0, Value::CreateStringValue("chrome://*/*"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidMatch));

  // Test missing and invalid files array
  input_value.reset(valid_value->DeepCopy());
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
  input_value.reset(valid_value->DeepCopy());
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));
  ListValue* permissions = NULL;
  input_value->GetList(keys::kPermissions, &permissions);
  ASSERT_FALSE(NULL == permissions);

  permissions = new ListValue;
  input_value->Set(keys::kPermissions, permissions);
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));

  input_value->Set(keys::kPermissions, Value::CreateIntegerValue(9));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPermissions));

  input_value.reset(valid_value->DeepCopy());
  input_value->GetList(keys::kPermissions, &permissions);
  permissions->Set(0, Value::CreateIntegerValue(24));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidPermission));

  // We allow unknown API permissions, so this will be valid until we better
  // distinguish between API and host permissions.
  permissions->Set(0, Value::CreateStringValue("www.google.com"));
  EXPECT_TRUE(extension.InitFromValue(*input_value, true, &error));

  // Multiple page actions are not allowed.
  input_value.reset(valid_value->DeepCopy());
  DictionaryValue* action = new DictionaryValue;
  action->SetString(keys::kPageActionId, "MyExtensionActionId");
  action->SetString(keys::kName, "MyExtensionActionName");
  ListValue* action_list = new ListValue;
  action_list->Append(action->DeepCopy());
  action_list->Append(action);
  input_value->Set(keys::kPageActions, action_list);
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_STREQ(errors::kInvalidPageActionsListSize, error.c_str());

  // Test invalid options page url.
  input_value.reset(valid_value->DeepCopy());
  input_value->Set(keys::kOptionsPage, Value::CreateNullValue());
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidOptionsPage));

  // Test invalid/empty default locale.
  input_value.reset(valid_value->DeepCopy());
  input_value->Set(keys::kDefaultLocale, Value::CreateIntegerValue(5));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidDefaultLocale));

  input_value->Set(keys::kDefaultLocale, Value::CreateStringValue(""));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidDefaultLocale));

  // Test invalid minimum_chrome_version.
  input_value.reset(valid_value->DeepCopy());
  input_value->Set(keys::kMinimumChromeVersion, Value::CreateIntegerValue(42));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kInvalidMinimumChromeVersion));

#if !defined(OS_MACOSX)
  // TODO(aa): The version isn't stamped into the unit test binary on mac.
  input_value->Set(keys::kMinimumChromeVersion,
                   Value::CreateStringValue("88.8"));
  EXPECT_FALSE(extension.InitFromValue(*input_value, true, &error));
  EXPECT_TRUE(MatchPattern(error, errors::kChromeVersionTooLow));
#endif
}

TEST(ExtensionTest, InitFromValueValid) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  scoped_refptr<Extension> extension_ptr(new Extension(path,
                                                       Extension::INVALID));
  Extension& extension = *extension_ptr;
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

  // Test permissions scheme.
  ListValue* permissions = new ListValue;
  permissions->Set(0, Value::CreateStringValue("file:///C:/foo.txt"));
  input_value.Set(keys::kPermissions, permissions);

  // We allow unknown API permissions, so this will be valid until we better
  // distinguish between API and host permissions.
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  input_value.Remove(keys::kPermissions, NULL);

  // Test with an options page.
  input_value.SetString(keys::kOptionsPage, "options.html");
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  EXPECT_EQ("chrome-extension", extension.options_url().scheme());
  EXPECT_EQ("/options.html", extension.options_url().path());

  // Test that an empty list of page actions does not stop a browser action
  // from being loaded.
  ListValue* empty_list = new ListValue;
  input_value.Set(keys::kPageActions, empty_list);
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);

#if !defined(OS_MACOSX)
  // TODO(aa): The version isn't stamped into the unit test binary on mac.
  // Test with a minimum_chrome_version.
  input_value.SetString(keys::kMinimumChromeVersion, "1.0");
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  // The minimum chrome version is not stored in the Extension object.
#endif
}

TEST(ExtensionTest, InitFromValueValidNameInRTL) {
#if defined(TOOLKIT_GTK)
  GtkTextDirection gtk_dir = gtk_widget_get_default_direction();
  gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);
#else
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");
#endif

#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  scoped_refptr<Extension> extension_ptr(new Extension(path,
                                                       Extension::INVALID));
  Extension& extension = *extension_ptr;
  std::string error;
  DictionaryValue input_value;

  input_value.SetString(keys::kVersion, "1.0.0.0");
  // No strong RTL characters in name.
  std::wstring name(L"Dictionary (by Google)");
  input_value.SetString(keys::kName, WideToUTF16Hack(name));
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  std::wstring localized_name(name);
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  EXPECT_EQ(localized_name, UTF8ToWide(extension.name()));

  // Strong RTL characters in name.
  name = L"Dictionary (\x05D1\x05D2"L" Google)";
  input_value.SetString(keys::kName, WideToUTF16Hack(name));
  EXPECT_TRUE(extension.InitFromValue(input_value, false, &error));
  EXPECT_EQ("", error);
  localized_name = name;
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  EXPECT_EQ(localized_name, UTF8ToWide(extension.name()));

  // Reset locale.
#if defined(TOOLKIT_GTK)
  gtk_widget_set_default_direction(gtk_dir);
#else
  base::i18n::SetICUDefaultLocale(locale);
#endif
}

TEST(ExtensionTest, GetResourceURLAndPath) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  DictionaryValue input_value;
  input_value.SetString(keys::kVersion, "1.0.0.0");
  input_value.SetString(keys::kName, "my extension");
  scoped_refptr<Extension> extension(Extension::Create(
      path, Extension::INVALID, input_value, false, NULL));
  EXPECT_TRUE(extension.get());

  EXPECT_EQ(extension->url().spec() + "bar/baz.js",
            Extension::GetResourceURL(extension->url(), "bar/baz.js").spec());
  EXPECT_EQ(extension->url().spec() + "baz.js",
            Extension::GetResourceURL(extension->url(),
                                      "bar/../baz.js").spec());
  EXPECT_EQ(extension->url().spec() + "baz.js",
            Extension::GetResourceURL(extension->url(), "../baz.js").spec());
}

TEST(ExtensionTest, LoadPageActionHelper) {
#if defined(OS_WIN)
    FilePath path(base::StringPrintf(L"c:\\extension"));
#else
    FilePath path(base::StringPrintf("/extension"));
#endif
  scoped_refptr<Extension> extension_ptr(new Extension(path,
                                                       Extension::INVALID));
  Extension& extension = *extension_ptr;
  std::string error_msg;
  scoped_ptr<ExtensionAction> action;
  DictionaryValue input;

  // First try with an empty dictionary.
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(action != NULL);
  ASSERT_TRUE(error_msg.empty());

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
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(id, action->id());
  // No title, so fall back to name.
  ASSERT_EQ(name, action->GetTitle(1));
  ASSERT_EQ(2u, action->icon_paths()->size());
  ASSERT_EQ(img1, (*action->icon_paths())[0]);
  ASSERT_EQ(img2, (*action->icon_paths())[1]);

  // Explicitly set the same type and parse again.
  input.SetString(keys::kType, values::kPageActionTypeTab);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(error_msg.empty());

  // Make a deep copy of the input and remove one key at a time and see if we
  // get the right error.
  scoped_ptr<DictionaryValue> copy;

  // First remove id key.
  copy.reset(input.DeepCopy());
  copy->Remove(keys::kPageActionId, NULL);
  action.reset(extension.LoadExtensionActionHelper(copy.get(), &error_msg));
  ASSERT_TRUE(NULL != action.get());

  // Then remove the name key. It's optional, so no error.
  copy.reset(input.DeepCopy());
  copy->Remove(keys::kName, NULL);
  action.reset(extension.LoadExtensionActionHelper(copy.get(), &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(action->GetTitle(1).empty());
  ASSERT_TRUE(error_msg.empty());

  // Then remove the icon paths key.
  copy.reset(input.DeepCopy());
  copy->Remove(keys::kPageActionIcons, NULL);
  action.reset(extension.LoadExtensionActionHelper(copy.get(), &error_msg));
  ASSERT_TRUE(NULL != action.get());
  error_msg = "";

  // Now test that we can parse the new format for page actions.

  // Now setup some values to use in the page action.
  const std::string kTitle("MyExtensionActionTitle");
  const std::string kIcon("image1.png");
  const std::string kPopupHtmlFile("a_popup.html");

  // Add the dictionary for the contextual action.
  input.Clear();
  input.SetString(keys::kPageActionDefaultTitle, kTitle);
  input.SetString(keys::kPageActionDefaultIcon, kIcon);

  // Parse and read back the values from the object.
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(action.get());
  ASSERT_TRUE(error_msg.empty());
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
  ASSERT_TRUE(error_msg.empty());

  input.Remove(keys::kPageActionDefaultTitle, NULL);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL == action.get());
  ASSERT_STREQ(errors::kInvalidPageActionName, error_msg.c_str());
  error_msg = "";

  // Test that keys "popup" and "default_popup" both work, but can not
  // be used at the same time.
  input.Clear();
  input.SetString(keys::kPageActionDefaultTitle, kTitle);
  input.SetString(keys::kPageActionDefaultIcon, kIcon);

  // LoadExtensionActionHelper expects the extension member |extension_url|
  // to be set.
  extension.extension_url_ =
      GURL(std::string(chrome::kExtensionScheme) +
           chrome::kStandardSchemeSeparator +
           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/");

  // Add key "popup", expect success.
  input.SetString(keys::kPageActionPopup, kPopupHtmlFile);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(error_msg.empty());
  ASSERT_STREQ(
      extension.url().Resolve(kPopupHtmlFile).spec().c_str(),
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());

  // Add key "default_popup", expect failure.
  input.SetString(keys::kPageActionDefaultPopup, kPopupHtmlFile);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL == action.get());
  ASSERT_STREQ(
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidPageActionOldAndNewKeys,
          keys::kPageActionDefaultPopup,
          keys::kPageActionPopup).c_str(),
      error_msg.c_str());
  error_msg = "";

  // Remove key "popup", expect success.
  input.Remove(keys::kPageActionPopup, NULL);
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(error_msg.empty());
  ASSERT_STREQ(
      extension.url().Resolve(kPopupHtmlFile).spec().c_str(),
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());

  // Setting default_popup to "" is the same as having no popup.
  input.Remove(keys::kPageActionDefaultPopup, NULL);
  input.SetString(keys::kPageActionDefaultPopup, "");
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(error_msg.empty());
  EXPECT_FALSE(action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ(
      "",
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());

  // Setting popup to "" is the same as having no popup.
  input.Remove(keys::kPageActionDefaultPopup, NULL);
  input.SetString(keys::kPageActionPopup, "");
  action.reset(extension.LoadExtensionActionHelper(&input, &error_msg));
  ASSERT_TRUE(NULL != action.get());
  ASSERT_TRUE(error_msg.empty());
  EXPECT_FALSE(action->HasPopup(ExtensionAction::kDefaultTabId));
  ASSERT_STREQ(
      "",
      action->GetPopupUrl(ExtensionAction::kDefaultTabId).spec().c_str());
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
    // (Why %Iu below?  This is the single file in the whole code base that
    // might make use of a WidePRIuS; let's not encourage any more.)
    FilePath path(base::StringPrintf(L"c:\\extension%Iu", i));
#else
    FilePath path(base::StringPrintf("/extension%" PRIuS, i));
#endif
    std::string error;

    input_value.SetString(keys::kVersion, "1.0");
    input_value.SetString(keys::kName, "Test");
    input_value.SetString(keys::kUpdateURL, url.spec());

    scoped_refptr<Extension> extension(Extension::Create(
        path, Extension::INVALID, input_value, false, &error));
    EXPECT_TRUE(extension.get()) << error;
  }

  // Test some invalid update urls
  std::vector<std::string> invalid;
  invalid.push_back("");
  invalid.push_back("test.com");
  valid.push_back("http://test.com/update#whatever");
  for (size_t i = 0; i < invalid.size(); i++) {
    DictionaryValue input_value;
#if defined(OS_WIN)
    // (Why %Iu below?  This is the single file in the whole code base that
    // might make use of a WidePRIuS; let's not encourage any more.)
    FilePath path(base::StringPrintf(L"c:\\extension%Iu", i));
#else
    FilePath path(base::StringPrintf("/extension%" PRIuS, i));
#endif
    std::string error;
    input_value.SetString(keys::kVersion, "1.0");
    input_value.SetString(keys::kName, "Test");
    input_value.SetString(keys::kUpdateURL, invalid[i]);

    scoped_refptr<Extension> extension(Extension::Create(
        path, Extension::INVALID, input_value, false, &error));
    EXPECT_FALSE(extension.get());
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

static scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                             const std::string& test_file) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueSerializer serializer(path);
  std::string error;
  scoped_ptr<Value> result(serializer.Deserialize(NULL, &error));
  if (!result.get()) {
    EXPECT_EQ("", error);
    return NULL;
  }

  scoped_refptr<Extension> extension = Extension::Create(
      path.DirName(), Extension::INVALID,
      *static_cast<DictionaryValue*>(result.get()), false, &error);
  EXPECT_TRUE(extension) << error;
  return extension;
}

TEST(ExtensionTest, EffectiveHostPermissions) {
  scoped_refptr<Extension> extension;
  ExtensionExtent hosts;

  extension = LoadManifest("effective_host_permissions", "empty.json");
  EXPECT_EQ(0u, extension->GetEffectiveHostPermissions().patterns().size());
  EXPECT_FALSE(hosts.ContainsURL(GURL("http://www.google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "one_host.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.google.com")));
  EXPECT_FALSE(hosts.ContainsURL(GURL("https://www.google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "one_host_wildcard.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://foo.google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "two_hosts.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.google.com")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.reddit.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "https_not_considered.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("https://google.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "two_content_scripts.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.reddit.com")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://news.ycombinator.com")));
  EXPECT_FALSE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://test/")));
  EXPECT_FALSE(hosts.ContainsURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts2.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts3.json");
  hosts = extension->GetEffectiveHostPermissions();
  EXPECT_FALSE(hosts.ContainsURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.ContainsURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());
}

TEST(ExtensionTest, IsPrivilegeIncrease) {
  const struct {
    const char* base_name;
    // Increase these sizes if you have more than 10.
    const char* granted_apis[10];
    const char* granted_hosts[10];
    bool full_access;
    bool expect_increase;
  } kTests[] = {
    { "allhosts1", {NULL}, {"http://*/", NULL}, false,
      false },  // all -> all
    { "allhosts2", {NULL}, {"http://*/", NULL}, false,
      false },  // all -> one
    { "allhosts3", {NULL}, {NULL}, false, true },  // one -> all
    { "hosts1", {NULL},
      {"http://www.google.com/", "http://www.reddit.com/", NULL}, false,
      false },  // http://a,http://b -> http://a,http://b
    { "hosts2", {NULL},
      {"http://www.google.com/", "http://www.reddit.com/", NULL}, false,
      true },  // http://a,http://b -> https://a,http://*.b
    { "hosts3", {NULL},
      {"http://www.google.com/", "http://www.reddit.com/", NULL}, false,
      false },  // http://a,http://b -> http://a
    { "hosts4", {NULL},
      {"http://www.google.com/", NULL}, false,
      true },  // http://a -> http://a,http://b
    { "hosts5", {"tabs", "notifications", NULL},
      {"http://*.example.com/", "http://*.example.com/*",
       "http://*.example.co.uk/*", "http://*.example.com.au/*",
       NULL}, false,
      false },  // http://a,b,c -> http://a,b,c + https://a,b,c
    { "hosts6", {"tabs", "notifications", NULL},
      {"http://*.example.com/", "http://*.example.com/*", NULL}, false,
      false },  // http://a.com -> http://a.com + http://a.co.uk
    { "permissions1", {"tabs", NULL},
      {NULL}, false, false },  // tabs -> tabs
    { "permissions2", {"tabs", NULL},
      {NULL}, false, true },  // tabs -> tabs,bookmarks
    { "permissions3", {NULL},
      {"http://*/*", NULL},
      false, true },  // http://a -> http://a,tabs
    { "permissions5", {"bookmarks", NULL},
      {NULL}, false, true },  // bookmarks -> bookmarks,history
#if !defined(OS_CHROMEOS)  // plugins aren't allowed in ChromeOS
    { "permissions4", {NULL},
      {NULL}, true, false },  // plugin -> plugin,tabs
    { "plugin1", {NULL},
      {NULL}, true, false },  // plugin -> plugin
    { "plugin2", {NULL},
      {NULL}, true, false },  // plugin -> none
    { "plugin3", {NULL},
      {NULL}, false, true },  // none -> plugin
#endif
    { "storage", {NULL},
      {NULL}, false, false },  // none -> storage
    { "notifications", {NULL},
      {NULL}, false, false }  // none -> notifications
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    scoped_refptr<Extension> old_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_old.json"));
    scoped_refptr<Extension> new_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_new.json"));

    std::set<std::string> granted_apis;
    for (size_t j = 0; kTests[i].granted_apis[j] != NULL; ++j)
      granted_apis.insert(kTests[i].granted_apis[j]);

    ExtensionExtent granted_hosts;
    for (size_t j = 0; kTests[i].granted_hosts[j] != NULL; ++j)
      AddPattern(&granted_hosts, kTests[i].granted_hosts[j]);

    EXPECT_TRUE(new_extension.get()) << kTests[i].base_name << "_new.json";
    if (!new_extension.get())
      continue;

    EXPECT_EQ(kTests[i].expect_increase,
              Extension::IsPrivilegeIncrease(kTests[i].full_access,
                                             granted_apis,
                                             granted_hosts,
                                             new_extension.get()))
        << kTests[i].base_name;
  }
}

TEST(ExtensionTest, PermissionMessages) {
  // Ensure that all permissions that needs to show install UI actually have
  // strings associated with them.

  std::set<std::string> skip;

  // These are considered "nuisance" or "trivial" permissions that don't need
  // a prompt.
  skip.insert(Extension::kContextMenusPermission);
  skip.insert(Extension::kIdlePermission);
  skip.insert(Extension::kNotificationPermission);
  skip.insert(Extension::kUnlimitedStoragePermission);
  skip.insert(Extension::kContentSettingsPermission);

  // TODO(erikkay) add a string for this permission.
  skip.insert(Extension::kBackgroundPermission);

  // The cookie permission does nothing unless you have associated host
  // permissions.
  skip.insert(Extension::kCookiePermission);

  // The proxy permission is warned as part of host permission checks.
  skip.insert(Extension::kProxyPermission);

  // If you've turned on the experimental command-line flag, we don't need
  // to warn you further.
  skip.insert(Extension::kExperimentalPermission);

  // This is only usable by component extensions.
  skip.insert(Extension::kWebstorePrivatePermission);

  for (size_t i = 0; i < Extension::kNumPermissions; ++i) {
    int message_id = Extension::kPermissions[i].message_id;
    std::string name = Extension::kPermissions[i].name;
    if (skip.count(name))
      EXPECT_EQ(0, message_id) << "unexpected message_id for " << name;
    else
      EXPECT_NE(0, message_id) << "missing message_id for " << name;
  }
}

// Returns a copy of |source| resized to |size| x |size|.
static SkBitmap ResizedCopy(const SkBitmap& source, int size) {
  return skia::ImageOperations::Resize(source,
                                       skia::ImageOperations::RESIZE_LANCZOS3,
                                       size,
                                       size);
}

static bool SizeEquals(const SkBitmap& bitmap, const gfx::Size& size) {
  return bitmap.width() == size.width() && bitmap.height() == size.height();
}

TEST(ExtensionTest, ImageCaching) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions");

  // Initialize the Extension.
  std::string errors;
  DictionaryValue values;
  values.SetString(keys::kName, "test");
  values.SetString(keys::kVersion, "0.1");
  scoped_refptr<Extension> extension(Extension::Create(
      path, Extension::INVALID, values, false, &errors));
  ASSERT_TRUE(extension.get());

  // Create an ExtensionResource pointing at an icon.
  FilePath icon_relative_path(FILE_PATH_LITERAL("icon3.png"));
  ExtensionResource resource(extension->id(),
                             extension->path(),
                             icon_relative_path);

  // Read in the icon file.
  FilePath icon_absolute_path = extension->path().Append(icon_relative_path);
  std::string raw_png;
  ASSERT_TRUE(file_util::ReadFileToString(icon_absolute_path, &raw_png));
  SkBitmap image;
  ASSERT_TRUE(gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(raw_png.data()),
      raw_png.length(),
      &image));

  // Make sure the icon file is the size we expect.
  gfx::Size original_size(66, 66);
  ASSERT_EQ(image.width(), original_size.width());
  ASSERT_EQ(image.height(), original_size.height());

  // Create two resized versions at size 16x16 and 24x24.
  SkBitmap image16 = ResizedCopy(image, 16);
  SkBitmap image24 = ResizedCopy(image, 24);

  gfx::Size size16(16, 16);
  gfx::Size size24(24, 24);

  // Cache the 16x16 copy.
  EXPECT_FALSE(extension->HasCachedImage(resource, size16));
  extension->SetCachedImage(resource, image16, original_size);
  EXPECT_TRUE(extension->HasCachedImage(resource, size16));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, size16), size16));
  EXPECT_FALSE(extension->HasCachedImage(resource, size24));
  EXPECT_FALSE(extension->HasCachedImage(resource, original_size));

  // Cache the 24x24 copy.
  extension->SetCachedImage(resource, image24, original_size);
  EXPECT_TRUE(extension->HasCachedImage(resource, size24));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, size24), size24));
  EXPECT_FALSE(extension->HasCachedImage(resource, original_size));

  // Cache the original, and verify that it gets returned when we ask for a
  // max_size that is larger than the original.
  gfx::Size size128(128, 128);
  EXPECT_TRUE(image.width() < size128.width() &&
              image.height() < size128.height());
  extension->SetCachedImage(resource, image, original_size);
  EXPECT_TRUE(extension->HasCachedImage(resource, original_size));
  EXPECT_TRUE(extension->HasCachedImage(resource, size128));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, original_size),
                         original_size));
  EXPECT_TRUE(SizeEquals(extension->GetCachedImage(resource, size128),
                         original_size));
  EXPECT_EQ(extension->GetCachedImage(resource, original_size).getPixels(),
            extension->GetCachedImage(resource, size128).getPixels());
}

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST(ExtensionTest, OldUnlimitedStoragePermission) {
  ScopedTempDir directory;
  ASSERT_TRUE(directory.CreateUniqueTempDir());
  FilePath extension_path = directory.path();
  DictionaryValue dictionary;

  // The two required keys.
  dictionary.SetString(extension_manifest_keys::kName, "test");
  dictionary.SetString(extension_manifest_keys::kVersion, "0.1");

  // Create a permissions list containing "unlimited_storage" and add it.
  ListValue* permissions = new ListValue();
  const char* old_unlimited = "unlimited_storage";
  EXPECT_STREQ(old_unlimited, Extension::kOldUnlimitedStoragePermission);
  permissions->Append(Value::CreateStringValue(old_unlimited));
  dictionary.Set(extension_manifest_keys::kPermissions, permissions);

  // Initialize the extension and make sure the permission for unlimited storage
  // is present.
  std::string errors;
  scoped_refptr<Extension> extension(Extension::Create(
      extension_path, Extension::INVALID, dictionary, false, &errors));
  EXPECT_TRUE(extension.get());
  EXPECT_TRUE(extension->HasApiPermission(
      Extension::kUnlimitedStoragePermission));
}

// This tests the API permissions with an empty manifest (one that just
// specifies a name and a version and nothing else).
TEST(ExtensionTest, ApiPermissions) {
  const struct {
    const char* permission_name;
    bool expect_success;
  } kTests[] = {
    // Negative test.
    { "non_existing_permission", false },
    // Test default module/package permission.
    { "browserAction",  true },
    { "browserActions", true },
    { "devtools",       true },
    { "extension",      true },
    { "i18n",           true },
    { "pageAction",     true },
    { "pageActions",    true },
    { "test",           true },
    // Some negative tests.
    { "bookmarks",      false },
    { "cookies",        false },
    { "history",        false },
    { "tabs.onUpdated", false },
    // Make sure we find the module name after stripping '.' and '/'.
    { "browserAction/abcd/onClick",  true },
    { "browserAction.abcd.onClick",  true },
    // Test Tabs functions.
    { "tabs.create",      true},
    { "tabs.update",      true},
    { "tabs.getSelected", false},
  };

  scoped_refptr<Extension> extension;
  extension = LoadManifest("empty_manifest", "empty.json");

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTests); ++i) {
    EXPECT_EQ(kTests[i].expect_success,
              extension->HasApiPermission(kTests[i].permission_name))
                  << "Permission being tested: " << kTests[i].permission_name;
  }
}

TEST(ExtensionTest, GetDistinctHostsForDisplay) {
  std::vector<std::string> expected;
  expected.push_back("www.foo.com");
  expected.push_back("www.bar.com");
  expected.push_back("www.baz.com");
  URLPatternList actual;

  {
    SCOPED_TRACE("no dupes");

    // Simple list with no dupes.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }

  {
    SCOPED_TRACE("two dupes");

    // Add some dupes.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }

  {
    SCOPED_TRACE("schemes differ");

    // Add a pattern that differs only by scheme. This should be filtered out.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTPS, "https://www.bar.com/path"));
    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }

  {
    SCOPED_TRACE("paths differ");

    // Add some dupes by path.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/pathypath"));
    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }

  {
    SCOPED_TRACE("subdomains differ");

    // We don't do anything special for subdomains.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://monkey.www.bar.com/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://bar.com/path"));

    expected.push_back("monkey.www.bar.com");
    expected.push_back("bar.com");

    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }

  {
    SCOPED_TRACE("RCDs differ");

    // Now test for RCD uniquing.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.de/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca.us/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com.my/path"));

    // This is an unknown RCD, which shouldn't be uniqued out.
    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.xyzzy/path"));

    expected.push_back("www.foo.xyzzy");

    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }

  {
    SCOPED_TRACE("wildcards");

    actual.push_back(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com/*"));

    expected.push_back("*.google.com");

    CompareLists(expected,
                 Extension::GetDistinctHostsForDisplay(actual));
  }
}

TEST(ExtensionTest, IsElevatedHostList) {
  URLPatternList list1;
  URLPatternList list2;

  list1.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"));
  list1.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));

  // Test that the host order does not matter.
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"));

  EXPECT_FALSE(Extension::IsElevatedHostList(list1, list2));
  EXPECT_FALSE(Extension::IsElevatedHostList(list2, list1));

  // Test that paths are ignored.
  list2.clear();
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/*"));
  EXPECT_FALSE(Extension::IsElevatedHostList(list1, list2));
  EXPECT_FALSE(Extension::IsElevatedHostList(list2, list1));

  // Test that RCDs are ignored.
  list2.clear();
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com.hk/*"));
  EXPECT_FALSE(Extension::IsElevatedHostList(list1, list2));
  EXPECT_FALSE(Extension::IsElevatedHostList(list2, list1));

  // Test that subdomain wildcards are handled properly.
  list2.clear();
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com.hk/*"));
  EXPECT_TRUE(Extension::IsElevatedHostList(list1, list2));
  //TODO(jstritar): Does not match subdomains properly. http://crbug.com/65337
  //EXPECT_FALSE(Extension::IsElevatedHostList(list2, list1));

  // Test that different domains count as different hosts.
  list2.clear();
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.google.com/path"));
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.example.org/path"));
  EXPECT_TRUE(Extension::IsElevatedHostList(list1, list2));
  EXPECT_FALSE(Extension::IsElevatedHostList(list2, list1));

  // Test that different subdomains count as different hosts.
  list2.clear();
  list2.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://mail.google.com/*"));
  EXPECT_TRUE(Extension::IsElevatedHostList(list1, list2));
  EXPECT_TRUE(Extension::IsElevatedHostList(list2, list1));
}

TEST(ExtensionTest, GenerateId) {
  std::string result;
  EXPECT_FALSE(Extension::GenerateId("", &result));

  EXPECT_TRUE(Extension::GenerateId("test", &result));
  EXPECT_EQ(result, "jpignaibiiemhngfjkcpokkamffknabf");

  EXPECT_TRUE(Extension::GenerateId("_", &result));
  EXPECT_EQ(result, "ncocknphbhhlhkikpnnlmbcnbgdempcd");

  EXPECT_TRUE(Extension::GenerateId(
      "this_string_is_longer_than_a_single_sha256_hash_digest", &result));
  EXPECT_EQ(result, "jimneklojkjdibfkgiiophfhjhbdgcfi");
}
