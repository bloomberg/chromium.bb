// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/preferences_browsertest.h"

#include <iostream>
#include <sstream>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#endif

using testing::Return;

namespace base {

// Helper for using EXPECT_EQ() with base::Value.
bool operator==(const base::Value& first, const base::Value& second) {
  return first.Equals(&second);
}

// Helper for pretty-printing the contents of base::Value in case of failures.
void PrintTo(const Value& value, std::ostream* stream) {
  std::string json;
  JSONWriter::Write(&value, &json);
  *stream << json;
}

}  // namespace base

PreferencesBrowserTest::PreferencesBrowserTest() {
}

// Sets up a mock user policy provider.
void PreferencesBrowserTest::SetUpInProcessBrowserTestFixture() {
  EXPECT_CALL(policy_provider_, IsInitializationComplete())
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);
};

// Navigates to the settings page, causing the JS preference handling classes to
// be loaded and initialized.
void PreferencesBrowserTest::SetUpOnMainThread() {
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUISettingsFrameURL));
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents);
  render_view_host_ = web_contents->GetRenderViewHost();
  ASSERT_TRUE(render_view_host_);
}

void PreferencesBrowserTest::VerifyValue(const base::DictionaryValue* dict,
                                         const std::string& key,
                                         base::Value* expected) {
  const base::Value* actual = NULL;
  EXPECT_TRUE(dict->Get(key, &actual)) << "Was checking key: " << key;
  EXPECT_EQ(*expected, *actual) << "Was checking key: " << key;
  delete expected;
}

void PreferencesBrowserTest::VerifyDict(const base::DictionaryValue* dict,
                                        const base::Value* value,
                                        const std::string& controlledBy,
                                        bool disabled) {
  VerifyValue(dict, "value", value->DeepCopy());
  if (!controlledBy.empty()) {
    VerifyValue(dict, "controlledBy",
                base::Value::CreateStringValue(controlledBy));
  } else {
    EXPECT_FALSE(dict->HasKey("controlledBy"));
  }

  if (disabled)
    VerifyValue(dict, "disabled", base::Value::CreateBooleanValue(true));
  else if (dict->HasKey("disabled"))
    VerifyValue(dict, "disabled", base::Value::CreateBooleanValue(false));
}

void PreferencesBrowserTest::VerifyPref(const base::DictionaryValue* prefs,
                                        const std::string& name,
                                        const base::Value* value,
                                        const std::string& controlledBy,
                                        bool disabled) {
  const base::Value* pref;
  const base::DictionaryValue* dict;
  ASSERT_TRUE(prefs->Get(name, &pref));
  ASSERT_TRUE(pref->GetAsDictionary(&dict));
  VerifyDict(dict, value, controlledBy, disabled);
}

void PreferencesBrowserTest::VerifyPrefs(
    const base::DictionaryValue* prefs,
    const std::vector<std::string>& names,
    const std::vector<base::Value*>& values,
    const std::string& controlledBy,
    bool disabled) {
  ASSERT_EQ(names.size(), values.size());
  for (size_t i = 0; i < names.size(); ++i)
    VerifyPref(prefs, names[i], values[i], controlledBy, disabled);
}

void PreferencesBrowserTest::VerifySetPref(const std::string& name,
                                           const std::string& type,
                                           base::Value* set_value,
                                           base::Value* expected_value) {
  scoped_ptr<base::Value> set_value_ptr(set_value);
  scoped_ptr<base::Value> expected_value_ptr(expected_value);
  std::string set_value_json;
  base::JSONWriter::Write(set_value, &set_value_json);
  std::wstringstream javascript;
  javascript << "pref_changed_callback = function(notification) {"
             << "  if (notification[0] == '" << name.c_str() << "') {"
             << "    pref_changed_callback = function() {};"
             << "    window.domAutomationController.send("
             << "        JSON.stringify(notification[1]));"
             << "  }"
             << "};"
             << "chrome.send('observePrefs', ['pref_changed_callback',"
             << "    '" << name.c_str() << "']);"
             << "Preferences.set" << type.c_str() << "Pref("
             << "    '" << name.c_str() << "',"
             << "    " << set_value_json.c_str() << ");";
  std::string actual_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, L"", javascript.str(), &actual_json));

  base::Value* actual_value = base::JSONReader::Read(actual_json);
  const base::DictionaryValue* actual_dict;
  ASSERT_TRUE(actual_value);
  ASSERT_TRUE(actual_value->GetAsDictionary(&actual_dict));
  VerifyDict(actual_dict, expected_value ? expected_value : set_value,
             "", false);
}

void PreferencesBrowserTest::FetchPrefs(const std::vector<std::string>& names,
                                        base::DictionaryValue** prefs) {
  *prefs = NULL;
  base::ListValue args_list;
  args_list.Append(base::Value::CreateStringValue("prefs_fetched_callback"));
  for (std::vector<std::string>::const_iterator name = names.begin();
       name != names.end(); ++name)
    args_list.Append(base::Value::CreateStringValue(*name));
  std::string args_json;
  base::JSONWriter::Write(&args_list, &args_json);
  std::wstringstream javascript;
  javascript << "prefs_fetched_callback = function(dict) {"
             << "  window.domAutomationController.send(JSON.stringify(dict));"
             << "};"
             << "chrome.send('fetchPrefs', " << args_json.c_str() << ");";

  std::string fetched_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, L"", javascript.str(), &fetched_json));
  base::Value* fetched_value = base::JSONReader::Read(fetched_json);
  base::DictionaryValue* fetched_dict;
  ASSERT_TRUE(fetched_value);
  ASSERT_TRUE(fetched_value->GetAsDictionary(&fetched_dict));
  *prefs = fetched_dict;
}

void PreferencesBrowserTest::SetUserPolicies(
    const std::vector<std::string>& names,
    const std::vector<base::Value*>& values,
    policy::PolicyLevel level) {
  ASSERT_TRUE(names.size() == values.size());
  policy::PolicyMap map;
  for (size_t i = 0; i < names.size(); ++i)
    map.Set(names[i], level, policy::POLICY_SCOPE_USER, values[i]->DeepCopy());
  policy_provider_.UpdateChromePolicy(map);
}

void PreferencesBrowserTest::DeleteValues(std::vector<base::Value*>& values){
  for (std::vector<base::Value*>::iterator value = values.begin();
       value != values.end(); ++value)
    delete *value;
  values.clear();
}

// Verifies that pref values received by observer callbacks are decorated
// correctly when the prefs are set from JavaScript.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, SetPrefs) {
  // Prefs handled by CoreOptionsHandler.
  VerifySetPref(prefs::kAlternateErrorPagesEnabled, "Boolean",
                base::Value::CreateBooleanValue(false), NULL);
  VerifySetPref(prefs::kRestoreOnStartup, "Integer",
                base::Value::CreateIntegerValue(4), NULL);
  VerifySetPref(prefs::kEnterpriseWebStoreName, "String",
                base::Value::CreateStringValue("Store"), NULL);
  VerifySetPref(prefs::kEnterpriseWebStoreURL, "URL",
                base::Value::CreateStringValue("http://www.google.com"),
                base::Value::CreateStringValue("http://www.google.com/"));
}

// Verifies that pref values are decorated correctly when fetched from
// JavaScript.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, FetchPrefs) {
  // Pref names.
  std::vector<std::string> pref_names;
  // Boolean pref.
  pref_names.push_back(prefs::kAlternateErrorPagesEnabled);
  // Integer pref.
  pref_names.push_back(prefs::kRestoreOnStartup);
  // String pref.
  pref_names.push_back(prefs::kEnterpriseWebStoreName);
  // URL pref.
  pref_names.push_back(prefs::kEnterpriseWebStoreURL);
  // List pref.
  pref_names.push_back(prefs::kURLsToRestoreOnStartup);

  // Corresponding policy names.
  std::vector<std::string> policy_names;
  policy_names.push_back(policy::key::kAlternateErrorPagesEnabled);
  policy_names.push_back(policy::key::kRestoreOnStartup);
  policy_names.push_back(policy::key::kEnterpriseWebStoreName);
  policy_names.push_back(policy::key::kEnterpriseWebStoreURL);
  policy_names.push_back(policy::key::kRestoreOnStartupURLs);

  // Default values.
  std::vector<base::Value*> values;
  values.push_back(base::Value::CreateBooleanValue(true));
#if defined(OS_CHROMEOS)
  values.push_back(base::Value::CreateIntegerValue(1));
#else
  values.push_back(base::Value::CreateIntegerValue(5));
#endif
  values.push_back(base::Value::CreateStringValue(""));
  values.push_back(base::Value::CreateStringValue(""));
  values.push_back(new base::ListValue);

  // Verify default values are fetched and decorated correctly.
  base::DictionaryValue* fetched;
  FetchPrefs(pref_names, &fetched);
  if (fetched)
    VerifyPrefs(fetched, pref_names, values, "", false);
  delete fetched;

  // Set recommended values.
  DeleteValues(values);
  values.push_back(base::Value::CreateBooleanValue(false));
  values.push_back(base::Value::CreateIntegerValue(4));
  values.push_back(base::Value::CreateStringValue("Store"));
  values.push_back(base::Value::CreateStringValue("http://www.google.com"));
  base::ListValue* list = new base::ListValue;
  list->Append(base::Value::CreateStringValue("http://www.google.com"));
  list->Append(base::Value::CreateStringValue("http://example.com"));
  values.push_back(list);
  SetUserPolicies(policy_names, values, policy::POLICY_LEVEL_RECOMMENDED);

  // Verify recommended values are fetched and decorated correctly.
  FetchPrefs(pref_names, &fetched);
  if (fetched)
    VerifyPrefs(fetched, pref_names, values, "recommended", false);
  delete fetched;

  // Set mandatory values.
  SetUserPolicies(policy_names, values, policy::POLICY_LEVEL_MANDATORY);

  // Verify mandatory values are fetched and decorated correctly.
  FetchPrefs(pref_names, &fetched);
  if (fetched)
    VerifyPrefs(fetched, pref_names, values, "policy", true);
  delete fetched;

  DeleteValues(values);
}

#if defined(OS_CHROMEOS)
// Verifies that pref values handled by the CoreChromeOSOptionsHandler class
// are decorated correctly when requested from JavaScript.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, ChromeOSDeviceFetchPrefs) {
  // Pref names.
  std::vector<std::string> names;
  // Boolean pref.
  names.push_back(chromeos::kAccountsPrefAllowGuest);
  // String pref.
  names.push_back(chromeos::kReleaseChannel);
  // List pref.
  names.push_back(chromeos::kAccountsPrefUsers);

  // Default pref values.
  std::vector<base::Value*> values;
  values.push_back(base::Value::CreateBooleanValue(true));
  values.push_back(base::Value::CreateStringValue(""));
  values.push_back(new base::ListValue);

  // Verify default values are fetched and decorated correctly.
  base::DictionaryValue* fetched;
  FetchPrefs(names, &fetched);
  if (fetched)
    VerifyPrefs(fetched, names, values, "", true);
  delete fetched;

  // Set mandatory values.
  DeleteValues(values);
  values.push_back(base::Value::CreateBooleanValue(false));
  values.push_back(base::Value::CreateStringValue("stable-channel"));
  base::ListValue* list = new base::ListValue;
  list->Append(base::Value::CreateStringValue("me@google.com"));
  list->Append(base::Value::CreateStringValue("you@google.com"));
  values.push_back(list);
  ASSERT_EQ(names.size(), values.size());
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  for (size_t i = 0; i < names.size(); ++i)
    cros_settings->Set(names[i], *values[i]);

  // Verify mandatory values are fetched and decorated correctly.
  DeleteValues(values);
  values.push_back(base::Value::CreateBooleanValue(false));
  values.push_back(base::Value::CreateStringValue("stable-channel"));
  list = new base::ListValue;
  base::DictionaryValue* dict = new base::DictionaryValue;
  dict->SetString("username", "me@google.com");
  dict->SetString("name", "me@google.com");
  dict->SetString("email", "");
  dict->SetBoolean("owner", false);
  list->Append(dict);
  dict = new base::DictionaryValue;
  dict->SetString("username", "you@google.com");
  dict->SetString("name", "you@google.com");
  dict->SetString("email", "");
  dict->SetBoolean("owner", false);
  list->Append(dict);
  values.push_back(list);
  FetchPrefs(names, &fetched);
  // FIXME(bartfab): Find a way to simulate enterprise enrollment in browser
  // tests. Only if Chrome thinks that it is enrolled will the device prefs be
  // decorated with "controlledBy: policy".
  if (fetched)
    VerifyPrefs(fetched, names, values, "", true);
  delete fetched;

  DeleteValues(values);
}

// Verifies that pref values handled by the Chrome OS proxy settings parser are
// decorated correctly when requested from JavaScript.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, ChromeOSProxyFetchPrefs) {
  // Pref names.
  std::vector<std::string> names;
  // Boolean pref.
  names.push_back(chromeos::kProxySingle);
  // Integer pref.
  names.push_back(chromeos::kProxySingleHttpPort);
  // String pref.
  names.push_back(chromeos::kProxySingleHttp);
  // List pref.
  names.push_back(chromeos::kProxyIgnoreList);

  // Default values.
  std::vector<base::Value*> values;
  values.push_back(base::Value::CreateBooleanValue(false));
  values.push_back(base::Value::CreateStringValue(""));
  values.push_back(base::Value::CreateStringValue(""));
  values.push_back(new base::ListValue());

  // Verify default values are fetched and decorated correctly.
  base::DictionaryValue* fetched;
  FetchPrefs(names, &fetched);
  if (fetched)
    VerifyPrefs(fetched, names, values, "", false);
  delete fetched;

  // Set user-modified values.
  DeleteValues(values);
  values.push_back(base::Value::CreateBooleanValue(true));
  values.push_back(base::Value::CreateIntegerValue(8080));
  values.push_back(base::Value::CreateStringValue("127.0.0.1"));
  base::ListValue* list = new base::ListValue();
  list->Append(base::Value::CreateStringValue("www.google.com"));
  list->Append(base::Value::CreateStringValue("example.com"));
  values.push_back(list);
  ASSERT_EQ(names.size(), values.size());
  Profile* profile = browser()->profile();
  // Do not set the Boolean pref. It will toogle automatically.
  for (size_t i = 1; i < names.size(); ++i)
    chromeos::proxy_cros_settings_parser::SetProxyPrefValue(
        profile, names[i], values[i]->DeepCopy());

  // Verify user-modified values are fetched and decorated correctly.
  FetchPrefs(names, &fetched);
  if (fetched)
    VerifyPrefs(fetched, names, values, "", false);
  delete fetched;

  DeleteValues(values);
}
#endif
