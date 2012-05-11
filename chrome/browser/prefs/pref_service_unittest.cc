// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_observer_mock.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/data/resource.h"
#include "webkit/glue/webpreferences.h"

using content::BrowserThread;
using content::WebContentsTester;
using testing::_;
using testing::Mock;

TEST(PrefServiceTest, NoObserverFire) {
  TestingPrefService prefs;

  const char pref_name[] = "homepage";
  prefs.RegisterStringPref(pref_name, std::string());

  const char new_pref_value[] = "http://www.google.com/";
  PrefObserverMock obs;
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(pref_name, &obs);

  // This should fire the checks in PrefObserverMock::Observe.
  const StringValue expected_value(new_pref_value);
  obs.Expect(&prefs, pref_name, &expected_value);
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);

  // Setting the pref to the same value should not set the pref value a second
  // time.
  EXPECT_CALL(obs, Observe(_, _, _)).Times(0);
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);

  // Clearing the pref should cause the pref to fire.
  const StringValue expected_default_value("");
  obs.Expect(&prefs, pref_name, &expected_default_value);
  prefs.ClearPref(pref_name);
  Mock::VerifyAndClearExpectations(&obs);

  // Clearing the pref again should not cause the pref to fire.
  EXPECT_CALL(obs, Observe(_, _, _)).Times(0);
  prefs.ClearPref(pref_name);
  Mock::VerifyAndClearExpectations(&obs);
}

TEST(PrefServiceTest, HasPrefPath) {
  TestingPrefService prefs;

  const char path[] = "fake.path";

  // Shouldn't initially have a path.
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Register the path. This doesn't set a value, so the path still shouldn't
  // exist.
  prefs.RegisterStringPref(path, std::string());
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Set a value and make sure we have a path.
  prefs.SetString(path, "blah");
  EXPECT_TRUE(prefs.HasPrefPath(path));
}

TEST(PrefServiceTest, Observers) {
  const char pref_name[] = "homepage";

  TestingPrefService prefs;
  prefs.SetUserPref(pref_name, Value::CreateStringValue("http://www.cnn.com"));
  prefs.RegisterStringPref(pref_name, std::string());

  const char new_pref_value[] = "http://www.google.com/";
  const StringValue expected_new_pref_value(new_pref_value);
  PrefObserverMock obs;
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(pref_name, &obs);

  // This should fire the checks in PrefObserverMock::Observe.
  obs.Expect(&prefs, pref_name, &expected_new_pref_value);
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);

  // Now try adding a second pref observer.
  const char new_pref_value2[] = "http://www.youtube.com/";
  const StringValue expected_new_pref_value2(new_pref_value2);
  PrefObserverMock obs2;
  obs.Expect(&prefs, pref_name, &expected_new_pref_value2);
  obs2.Expect(&prefs, pref_name, &expected_new_pref_value2);
  registrar.Add(pref_name, &obs2);
  // This should fire the checks in obs and obs2.
  prefs.SetString(pref_name, new_pref_value2);
  Mock::VerifyAndClearExpectations(&obs);
  Mock::VerifyAndClearExpectations(&obs2);

  // Make sure obs2 still works after removing obs.
  registrar.Remove(pref_name, &obs);
  EXPECT_CALL(obs, Observe(_, _, _)).Times(0);
  obs2.Expect(&prefs, pref_name, &expected_new_pref_value);
  // This should only fire the observer in obs2.
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);
  Mock::VerifyAndClearExpectations(&obs2);
}

// Make sure that if a preference changes type, so the wrong type is stored in
// the user pref file, it uses the correct fallback value instead.
TEST(PrefServiceTest, GetValueChangedType) {
  const int kTestValue = 10;
  TestingPrefService prefs;
  prefs.RegisterIntegerPref(prefs::kStabilityLaunchCount, kTestValue);

  // Check falling back to a recommended value.
  prefs.SetUserPref(prefs::kStabilityLaunchCount,
                    Value::CreateStringValue("not an integer"));
  const PrefService::Preference* pref =
      prefs.FindPreference(prefs::kStabilityLaunchCount);
  ASSERT_TRUE(pref);
  const Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(Value::TYPE_INTEGER, value->GetType());
  int actual_int_value = -1;
  EXPECT_TRUE(value->GetAsInteger(&actual_int_value));
  EXPECT_EQ(kTestValue, actual_int_value);
}

TEST(PrefServiceTest, UpdateCommandLinePrefStore) {
  TestingPrefService prefs;
  prefs.RegisterBooleanPref(prefs::kCloudPrintProxyEnabled, false);

  // Check to make sure the value is as expected.
  const PrefService::Preference* pref =
      prefs.FindPreference(prefs::kCloudPrintProxyEnabled);
  ASSERT_TRUE(pref);
  const Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
  bool actual_bool_value = true;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_FALSE(actual_bool_value);

  // Change the command line.
  CommandLine cmd_line(CommandLine::NO_PROGRAM);
  cmd_line.AppendSwitch(switches::kEnableCloudPrintProxy);

  // Call UpdateCommandLinePrefStore and check to see if the value has changed.
  prefs.UpdateCommandLinePrefStore(&cmd_line);
  pref = prefs.FindPreference(prefs::kCloudPrintProxyEnabled);
  ASSERT_TRUE(pref);
  value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(Value::TYPE_BOOLEAN, value->GetType());
  actual_bool_value = false;
  EXPECT_TRUE(value->GetAsBoolean(&actual_bool_value));
  EXPECT_TRUE(actual_bool_value);
}

class PrefServiceUserFilePrefsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("pref_service");
    ASSERT_TRUE(file_util::PathExists(data_dir_));
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
  ScopedTempDir temp_dir_;
  // The path to the directory where the test data is stored.
  FilePath data_dir_;
  // A message loop that we can use as the file thread message loop.
  MessageLoop message_loop_;
};

// Verifies that ListValue and DictionaryValue pref with non emtpy default
// preserves its empty value.
TEST_F(PrefServiceUserFilePrefsTest, PreserveEmptyValue) {
  FilePath pref_file = temp_dir_.path().AppendASCII("write.json");

  ASSERT_TRUE(file_util::CopyFile(
      data_dir_.AppendASCII("read.need_empty_value.json"),
      pref_file));

  PrefServiceMockBuilder builder;
  builder.WithUserFilePrefs(pref_file, base::MessageLoopProxy::current());
  scoped_ptr<PrefService> prefs(builder.Create());

  // Register testing prefs.
  prefs->RegisterListPref("list",
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref("dict",
                                PrefService::UNSYNCABLE_PREF);

  base::ListValue* non_empty_list = new base::ListValue;
  non_empty_list->Append(base::Value::CreateStringValue("test"));
  prefs->RegisterListPref("list_needs_empty_value",
                          non_empty_list,
                          PrefService::UNSYNCABLE_PREF);

  base::DictionaryValue* non_empty_dict = new base::DictionaryValue;
  non_empty_dict->SetString("dummy", "whatever");
  prefs->RegisterDictionaryPref("dict_needs_empty_value",
                                non_empty_dict,
                                PrefService::UNSYNCABLE_PREF);

  // Set all testing prefs to empty.
  ClearListValue(prefs.get(), "list");
  ClearListValue(prefs.get(), "list_needs_empty_value");
  ClearDictionaryValue(prefs.get(), "dict");
  ClearDictionaryValue(prefs.get(), "dict_needs_empty_value");

  // Write to file.
  prefs->CommitPendingWrite();
  MessageLoop::current()->RunAllPending();

  // Compare to expected output.
  FilePath golden_output_file =
      data_dir_.AppendASCII("write.golden.need_empty_value.json");
  ASSERT_TRUE(file_util::PathExists(golden_output_file));
  EXPECT_TRUE(file_util::TextContentsEqual(golden_output_file, pref_file));
}

class PrefServiceSetValueTest : public testing::Test {
 protected:
  static const char kName[];
  static const char kValue[];

  TestingPrefService prefs_;
  PrefObserverMock observer_;
};

const char PrefServiceSetValueTest::kName[] = "name";
const char PrefServiceSetValueTest::kValue[] = "value";

TEST_F(PrefServiceSetValueTest, SetStringValue) {
  const char default_string[] = "default";
  const StringValue default_value(default_string);
  prefs_.RegisterStringPref(kName, default_string);

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, &observer_);

  // Changing the controlling store from default to user triggers notification.
  observer_.Expect(&prefs_, kName, &default_value);
  prefs_.Set(kName, default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  prefs_.Set(kName, default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  StringValue new_value(kValue);
  observer_.Expect(&prefs_, kName, &new_value);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(PrefServiceSetValueTest, SetDictionaryValue) {
  prefs_.RegisterDictionaryPref(kName);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, &observer_);

  EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  prefs_.RemoveUserPref(kName);
  Mock::VerifyAndClearExpectations(&observer_);

  DictionaryValue new_value;
  new_value.SetString(kName, kValue);
  observer_.Expect(&prefs_, kName, &new_value);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  DictionaryValue empty;
  observer_.Expect(&prefs_, kName, &empty);
  prefs_.Set(kName, empty);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(PrefServiceSetValueTest, SetListValue) {
  prefs_.RegisterListPref(kName);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, &observer_);

  EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  prefs_.RemoveUserPref(kName);
  Mock::VerifyAndClearExpectations(&observer_);

  ListValue new_value;
  new_value.Append(Value::CreateStringValue(kValue));
  observer_.Expect(&prefs_, kName, &new_value);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, Observe(_, _, _)).Times(0);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  ListValue empty;
  observer_.Expect(&prefs_, kName, &empty);
  prefs_.Set(kName, empty);
  Mock::VerifyAndClearExpectations(&observer_);
}

class PrefServiceWebKitPrefs : public ChromeRenderViewHostTestHarness {
 protected:
  PrefServiceWebKitPrefs() : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();

    // Supply our own profile so we use the correct profile data. The test
    // harness is not supposed to overwrite a profile if it's already created.

    // Set some (WebKit) user preferences.
    TestingPrefService* pref_services = profile()->GetTestingPrefService();
#if defined(TOOLKIT_GTK)
    pref_services->SetUserPref(prefs::kUsesSystemTheme,
                               Value::CreateBooleanValue(false));
#endif
    pref_services->SetUserPref(prefs::kGlobalDefaultCharset,
                               Value::CreateStringValue("utf8"));
    pref_services->SetUserPref(prefs::kWebKitGlobalDefaultFontSize,
                               Value::CreateIntegerValue(20));
    pref_services->SetUserPref(prefs::kWebKitTextAreasAreResizable,
                               Value::CreateBooleanValue(false));
    pref_services->SetUserPref(prefs::kWebKitUsesUniversalDetector,
                               Value::CreateBooleanValue(true));
    pref_services->SetUserPref("webkit.webprefs.foo",
                               Value::CreateStringValue("bar"));
  }

 private:
  content::TestBrowserThread ui_thread_;
};

// Tests to see that webkit preferences are properly loaded and copied over
// to a WebPreferences object.
TEST_F(PrefServiceWebKitPrefs, PrefsCopied) {
  webkit_glue::WebPreferences webkit_prefs =
      WebContentsTester::For(contents())->TestGetWebkitPrefs();

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
  EXPECT_EQ(ASCIIToUTF16(kDefaultFont), webkit_prefs.standard_font_family);
  EXPECT_TRUE(webkit_prefs.javascript_enabled);
}
