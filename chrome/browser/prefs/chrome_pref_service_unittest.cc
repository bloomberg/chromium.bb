// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/test/data/resource.h"

using content::WebContentsTester;
using content::WebPreferences;

TEST(ChromePrefServiceTest, UpdateCommandLinePrefStore) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled, false);

  // Check to make sure the value is as expected.
  const PrefService::Preference* pref =
      prefs.FindPreference(prefs::kCloudPrintProxyEnabled);
  ASSERT_TRUE(pref);
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, value->GetType());
  bool actual_bool_value = true;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_FALSE(actual_bool_value);

  // Change the command line.
  CommandLine cmd_line(CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(switches::kEnableCloudPrintProxy);

  // Call UpdateCommandLinePrefStore and check to see if the value has changed.
  prefs.UpdateCommandLinePrefStore(new CommandLinePrefStore(&cmd_line));
  pref = prefs.FindPreference(prefs::kCloudPrintProxyEnabled);
  ASSERT_TRUE(pref);
  value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, value->GetType());
  actual_bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_TRUE(actual_bool_value);
}

class ChromePrefServiceUserFilePrefsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("pref_service");
    ASSERT_TRUE(base::PathExists(data_dir_));
  }

  void ClearListValue(PrefService* prefs, const char* key) {
    ListPrefUpdate updater(prefs, key);
    updater->Clear();
  }

  void ClearDictionaryValue(PrefService* prefs, const char* key) {
    DictionaryPrefUpdate updater(prefs, key);
    updater->Clear();
  }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  // The path to the directory where the test data is stored.
  base::FilePath data_dir_;
  // A message loop that we can use as the file thread message loop.
  base::MessageLoop message_loop_;
};

class ChromePrefServiceWebKitPrefs : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();

    // Supply our own profile so we use the correct profile data. The test
    // harness is not supposed to overwrite a profile if it's already created.

    // Set some (WebKit) user preferences.
    TestingPrefServiceSyncable* pref_services =
        profile()->GetTestingPrefService();
    pref_services->SetUserPref(prefs::kDefaultCharset,
                               new base::StringValue("utf8"));
    pref_services->SetUserPref(prefs::kWebKitDefaultFontSize,
                               new base::FundamentalValue(20));
    pref_services->SetUserPref(prefs::kWebKitTextAreasAreResizable,
                               new base::FundamentalValue(false));
    pref_services->SetUserPref(prefs::kWebKitUsesUniversalDetector,
                               new base::FundamentalValue(true));
    pref_services->SetUserPref("webkit.webprefs.foo",
                               new base::StringValue("bar"));
  }
};

// Tests to see that webkit preferences are properly loaded and copied over
// to a WebPreferences object.
TEST_F(ChromePrefServiceWebKitPrefs, PrefsCopied) {
  WebPreferences webkit_prefs =
      WebContentsTester::For(web_contents())->TestComputeWebkitPrefs();

  // These values have been overridden by the profile preferences.
  EXPECT_EQ("UTF-8", webkit_prefs.default_encoding);
  EXPECT_EQ(20, webkit_prefs.default_font_size);
  EXPECT_FALSE(webkit_prefs.text_areas_are_resizable);
  EXPECT_TRUE(webkit_prefs.uses_universal_detector);

  // These should still be the default values.
#if defined(OS_MACOSX)
  const char kDefaultFont[] = "Times";
#elif defined(OS_CHROMEOS)
  const char kDefaultFont[] = "Tinos";
#else
  const char kDefaultFont[] = "Times New Roman";
#endif
  EXPECT_EQ(base::ASCIIToUTF16(kDefaultFont),
            webkit_prefs.standard_font_family_map[prefs::kWebKitCommonScript]);
  EXPECT_TRUE(webkit_prefs.javascript_enabled);
}
