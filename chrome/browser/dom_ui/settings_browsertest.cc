// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/web_ui_browsertest.h"
#include "chrome/browser/dom_ui/options/core_options_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;
using ::testing::_;

MATCHER_P(Eq_ListValue, inList, "") {
  return arg->Equals(inList);
}

class MockCoreOptionsHandler : public CoreOptionsHandler {
 public:
  MOCK_METHOD1(HandleInitialize,
      void(const ListValue* args));
  MOCK_METHOD1(HandleFetchPrefs,
      void(const ListValue* args));
  MOCK_METHOD1(HandleObservePrefs,
      void(const ListValue* args));
  MOCK_METHOD1(HandleSetBooleanPref,
      void(const ListValue* args));
  MOCK_METHOD1(HandleSetIntegerPref,
      void(const ListValue* args));
  MOCK_METHOD1(HandleSetDoublePref,
      void(const ListValue* args));
  MOCK_METHOD1(HandleSetStringPref,
      void(const ListValue* args));
  MOCK_METHOD1(HandleSetObjectPref,
      void(const ListValue* args));
  MOCK_METHOD1(HandleClearPref,
      void(const ListValue* args));
  MOCK_METHOD1(HandleUserMetricsAction,
      void(const ListValue* args));

  virtual void RegisterMessages() {
    dom_ui_->RegisterMessageCallback("coreOptionsInitialize",
        NewCallback(this, &MockCoreOptionsHandler ::HandleInitialize));
    dom_ui_->RegisterMessageCallback("fetchPrefs",
        NewCallback(this, &MockCoreOptionsHandler ::HandleFetchPrefs));
    dom_ui_->RegisterMessageCallback("observePrefs",
        NewCallback(this, &MockCoreOptionsHandler ::HandleObservePrefs));
    dom_ui_->RegisterMessageCallback("setBooleanPref",
        NewCallback(this, &MockCoreOptionsHandler ::HandleSetBooleanPref));
    dom_ui_->RegisterMessageCallback("setIntegerPref",
        NewCallback(this, &MockCoreOptionsHandler ::HandleSetIntegerPref));
    dom_ui_->RegisterMessageCallback("setDoublePref",
        NewCallback(this, &MockCoreOptionsHandler ::HandleSetDoublePref));
    dom_ui_->RegisterMessageCallback("setStringPref",
        NewCallback(this, &MockCoreOptionsHandler ::HandleSetStringPref));
    dom_ui_->RegisterMessageCallback("setObjectPref",
        NewCallback(this, &MockCoreOptionsHandler ::HandleSetObjectPref));
    dom_ui_->RegisterMessageCallback("clearPref",
        NewCallback(this, &MockCoreOptionsHandler ::HandleClearPref));
    dom_ui_->RegisterMessageCallback("coreOptionsUserMetricsAction",
        NewCallback(this, &MockCoreOptionsHandler ::HandleUserMetricsAction));
  }
};

class SettingsWebUITest : public WebUIBrowserTest {
 protected:
  virtual WebUIMessageHandler* GetMockMessageHandler() {
    return &mock_core_options_handler_;
  }

  StrictMock<MockCoreOptionsHandler> mock_core_options_handler_;
};

// Test the end to end js to WebUI handler code path for
// the message setBooleanPref.
// TODO(dtseng): add more EXPECT_CALL's when updating js test.
IN_PROC_BROWSER_TEST_F(SettingsWebUITest, TestSetBooleanPrefTriggers) {
  // This serves as an example of a very constrained test.
  ListValue true_list_value;
  true_list_value.Append(Value::CreateStringValue("browser.show_home_button"));
  true_list_value.Append(Value::CreateBooleanValue(true));
  true_list_value.Append(
      Value::CreateStringValue("Options_Homepage_HomeButton"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUISettingsURL));
  EXPECT_CALL(mock_core_options_handler_,
      HandleSetBooleanPref(Eq_ListValue(&true_list_value)));
  ASSERT_TRUE(RunWebUITest(
      FILE_PATH_LITERAL("settings_set_boolean_pref_triggers.js")));
}

