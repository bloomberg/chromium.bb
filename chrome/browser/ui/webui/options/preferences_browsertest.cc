// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/preferences_browsertest.h"

#include <iostream>
#include <sstream>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_service_base.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#endif

using testing::AllOf;
using testing::Mock;
using testing::Property;
using testing::Return;

namespace base {

// Helper for using EXPECT_EQ() with base::Value.
bool operator==(const Value& first, const Value& second) {
  return first.Equals(&second);
}

// Helper for pretty-printing the contents of base::Value in case of failures.
void PrintTo(const Value& value, std::ostream* stream) {
  std::string json;
  JSONWriter::Write(&value, &json);
  *stream << json;
}

}  // namespace base

// Googlemock matcher for base::Value.
MATCHER_P(EqualsValue, expected, "") {
  return arg && arg->Equals(expected);
}

PreferencesBrowserTest::PreferencesBrowserTest() {
}

PreferencesBrowserTest::~PreferencesBrowserTest() {
}

// Navigates to the settings page, causing the JavaScript pref handling code to
// load and injects JavaScript testing code.
void PreferencesBrowserTest::SetUpOnMainThread() {
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUISettingsFrameURL));
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(web_contents);
  render_view_host_ = web_contents->GetRenderViewHost();
  ASSERT_TRUE(render_view_host_);
  pref_change_registrar_.Init(
      PrefServiceBase::FromBrowserContext(browser()->profile()));
  pref_service_ = browser()->profile()->GetPrefs();
  ASSERT_TRUE(content::ExecuteJavaScript(render_view_host_,
      "",
      "function TestEnv() {"
      "  this.sentinelName_ = 'download.prompt_for_download';"
      "  this.prefs_ = [];"
      "  TestEnv.instance_ = this;"
      "}"
      ""
      "TestEnv.handleEvent = function(event) {"
      "  var env = TestEnv.instance_;"
      "  var name = event.type;"
      "  env.removePrefListener_(name);"
      "  if (name == TestEnv.sentinelName_)"
      "    env.sentinelValue_ = event.value.value;"
      "  else"
      "    env.reply_[name] = event.value;"
      "  if (env.fetching_ && !--env.fetching_ ||"
      "      !env.fetching_ && name == env.sentinelName_) {"
      "    env.removePrefListeners_();"
      "    window.domAutomationController.send(JSON.stringify(env.reply_));"
      "    delete env.reply_;"
      "  }"
      "};"
      ""
      "TestEnv.prototype = {"
      "  addPrefListener_: function(name) {"
      "    Preferences.getInstance().addEventListener(name,"
      "                                               TestEnv.handleEvent);"
      "  },"
      ""
      "  addPrefListeners_: function() {"
      "    for (var i in this.prefs_)"
      "      this.addPrefListener_(this.prefs_[i]);"
      "  },"
      ""
      "  removePrefListener_: function(name) {"
      "    Preferences.getInstance().removeEventListener(name,"
      "                                                  TestEnv.handleEvent);"
      "  },"
      ""
      "  removePrefListeners_: function() {"
      "    for (var i in this.prefs_)"
      "      this.removePrefListener_(this.prefs_[i]);"
      "  },"
      ""
      ""
      "  addPref: function(name) {"
      "    this.prefs_.push(name);"
      "  },"
      ""
      "  setupAndReply: function() {"
      "    this.reply_ = {};"
      "    Preferences.instance_ = new Preferences();"
      "    this.addPref(this.sentinelName_);"
      "    this.fetching_ = this.prefs_.length;"
      "    this.addPrefListeners_();"
      "    Preferences.getInstance().initialize();"
      "  },"
      ""
      "  runAndReply: function(test) {"
      "    this.reply_ = {};"
      "    this.addPrefListeners_();"
      "    test();"
      "    this.sentinelValue_ = !this.sentinelValue_;"
      "    Preferences.setBooleanPref(this.sentinelName_, this.sentinelValue_,"
      "                               true);"
      "  },"
      ""
      "  startObserving: function() {"
      "    this.reply_ = {};"
      "    this.addPrefListeners_();"
      "  },"
      ""
      "  finishObservingAndReply: function() {"
      "    this.sentinelValue_ = !this.sentinelValue_;"
      "    Preferences.setBooleanPref(this.sentinelName_, this.sentinelValue_,"
      "                               true);"
      "  }"
      "};"));
}

// Forwards notifications received when pref values change in the backend.
void PreferencesBrowserTest::OnPreferenceChanged(const std::string& pref_name) {
  OnCommit(pref_service_->FindPreference(pref_name.c_str()));
}

// Sets up a mock user policy provider.
void PreferencesBrowserTest::SetUpInProcessBrowserTestFixture() {
  EXPECT_CALL(policy_provider_, IsInitializationComplete())
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);
};

void PreferencesBrowserTest::TearDownInProcessBrowserTestFixture() {
  STLDeleteElements(&default_values_);
  STLDeleteElements(&non_default_values_);
}

void PreferencesBrowserTest::SetUserPolicies(
    const std::vector<std::string>& names,
    const std::vector<base::Value*>& values,
    policy::PolicyLevel level) {
  policy::PolicyMap map;
  for (size_t i = 0; i < names.size(); ++i)
    map.Set(names[i], level, policy::POLICY_SCOPE_USER, values[i]->DeepCopy());
  policy_provider_.UpdateChromePolicy(map);
}

void PreferencesBrowserTest::ClearUserPolicies() {
  policy::PolicyMap empty_policy_map;
  policy_provider_.UpdateChromePolicy(empty_policy_map);
}

void PreferencesBrowserTest::SetUserValues(
    const std::vector<std::string>& names,
    const std::vector<base::Value*>& values) {
  for (size_t i = 0; i < names.size(); ++i)
    pref_service_->Set(names[i].c_str(), *values[i]);
}

void PreferencesBrowserTest::VerifyKeyValue(const base::DictionaryValue& dict,
                                            const std::string& key,
                                            const base::Value& expected) {
  const base::Value* actual = NULL;
  EXPECT_TRUE(dict.Get(key, &actual)) << "Was checking key: " << key;
  EXPECT_EQ(expected, *actual) << "Was checking key: " << key;
}

void PreferencesBrowserTest::VerifyPref(const base::DictionaryValue* prefs,
                                        const std::string& name,
                                        const base::Value* value,
                                        const std::string& controlledBy,
                                        bool disabled,
                                        bool uncommitted) {
  const base::Value* pref;
  const base::DictionaryValue* dict;
  ASSERT_TRUE(prefs->GetWithoutPathExpansion(name, &pref));
  ASSERT_TRUE(pref->GetAsDictionary(&dict));
  VerifyKeyValue(*dict, "value", *value);
  if (!controlledBy.empty()) {
    VerifyKeyValue(*dict, "controlledBy", base::StringValue(controlledBy));
  } else {
    EXPECT_FALSE(dict->HasKey("controlledBy"));
  }
  if (disabled)
    VerifyKeyValue(*dict, "disabled", base::FundamentalValue(true));
  else if (dict->HasKey("disabled"))
    VerifyKeyValue(*dict, "disabled", base::FundamentalValue(false));
  if (uncommitted)
    VerifyKeyValue(*dict, "uncommitted", base::FundamentalValue(true));
  else if (dict->HasKey("uncommitted"))
    VerifyKeyValue(*dict, "uncommitted", base::FundamentalValue(false));
}

void PreferencesBrowserTest::VerifyObservedPref(const std::string& json,
                                                const std::string& name,
                                                const base::Value* value,
                                                const std::string& controlledBy,
                                                bool disabled,
                                                bool uncommitted) {
  scoped_ptr<base::Value> observed_value_ptr(base::JSONReader::Read(json));
  const base::DictionaryValue* observed_dict;
  ASSERT_TRUE(observed_value_ptr.get());
  ASSERT_TRUE(observed_value_ptr->GetAsDictionary(&observed_dict));
  VerifyPref(observed_dict, name, value, controlledBy, disabled, uncommitted);
}

void PreferencesBrowserTest::VerifyObservedPrefs(
    const std::string& json,
    const std::vector<std::string>& names,
    const std::vector<base::Value*>& values,
    const std::string& controlledBy,
    bool disabled,
    bool uncommitted) {
  scoped_ptr<base::Value> observed_value_ptr(base::JSONReader::Read(json));
  const base::DictionaryValue* observed_dict;
  ASSERT_TRUE(observed_value_ptr.get());
  ASSERT_TRUE(observed_value_ptr->GetAsDictionary(&observed_dict));
  for (size_t i = 0; i < names.size(); ++i)
    VerifyPref(observed_dict, names[i], values[i], controlledBy, disabled,
               uncommitted);
}

void PreferencesBrowserTest::ExpectNoCommit(const std::string& name) {
  pref_change_registrar_.Add(
      name.c_str(),
      base::Bind(&PreferencesBrowserTest::OnPreferenceChanged,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnCommit(Property(&PrefService::Preference::name, name)))
      .Times(0);
}

void PreferencesBrowserTest::ExpectSetCommit(const std::string& name,
                                             const base::Value* value) {
  pref_change_registrar_.Add(
      name.c_str(),
      base::Bind(&PreferencesBrowserTest::OnPreferenceChanged,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnCommit(AllOf(
      Property(&PrefService::Preference::name, name),
      Property(&PrefService::Preference::IsUserControlled, true),
      Property(&PrefService::Preference::GetValue, EqualsValue(value)))));
}

void PreferencesBrowserTest::ExpectClearCommit(const std::string& name) {
  pref_change_registrar_.Add(
      name.c_str(),
      base::Bind(&PreferencesBrowserTest::OnPreferenceChanged,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnCommit(AllOf(
      Property(&PrefService::Preference::name, name),
      Property(&PrefService::Preference::IsUserControlled, false))));
}

void PreferencesBrowserTest::VerifyAndClearExpectations() {
  Mock::VerifyAndClearExpectations(this);
  pref_change_registrar_.RemoveAll();
}

void PreferencesBrowserTest::SetupJavaScriptTestEnvironment(
    const std::vector<std::string>& pref_names,
    std::string* observed_json) const {
  std::stringstream javascript;
  javascript << "var testEnv = new TestEnv();";
  for (std::vector<std::string>::const_iterator name = pref_names.begin();
       name != pref_names.end(); ++name)
    javascript << "testEnv.addPref('" << name->c_str() << "');";
  javascript << "testEnv.setupAndReply();";
  std::string temp_observed_json;
  if (!observed_json)
    observed_json = &temp_observed_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, "", javascript.str(), observed_json));
}

void PreferencesBrowserTest::VerifySetPref(const std::string& name,
                                           const std::string& type,
                                           const base::Value* value,
                                           bool commit) {
  if (commit)
    ExpectSetCommit(name, value);
  else
    ExpectNoCommit(name);
  scoped_ptr<base::Value> commit_ptr(new base::FundamentalValue(commit));
  std::string value_json;
  std::string commit_json;
  base::JSONWriter::Write(value, &value_json);
  base::JSONWriter::Write(commit_ptr.get(), &commit_json);
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.set" << type.c_str() << "Pref("
             << "      '" << name.c_str() << "',"
             << "      " << value_json.c_str() << ","
             << "      " << commit_json.c_str() << ");});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, "", javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, "", false, !commit);
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyClearPref(const std::string& name,
                                             const base::Value* value,
                                             bool commit) {
  if (commit)
    ExpectClearCommit(name);
  else
    ExpectNoCommit(name);
  scoped_ptr<base::Value> commit_ptr(new base::FundamentalValue(commit));
  std::string commit_json;
  base::JSONWriter::Write(commit_ptr.get(), &commit_json);
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.clearPref("
             << "      '" << name.c_str() << "',"
             << "      " << commit_json.c_str() << ");});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, "", javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, "recommended", false, !commit);
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyCommit(const std::string& name,
                                          const base::Value* value,
                                          const std::string& controlledBy) {
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.getInstance().commitPref("
             << "        '" << name.c_str() << "');});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, "", javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, controlledBy, false, false);
}

void PreferencesBrowserTest::VerifySetCommit(const std::string& name,
                                             const base::Value* value) {
  ExpectSetCommit(name, value);
  VerifyCommit(name, value, "");
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyClearCommit(const std::string& name,
                                               const base::Value* value) {
  ExpectClearCommit(name);
  VerifyCommit(name, value, "recommended");
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyRollback(const std::string& name,
                                            const base::Value* value,
                                            const std::string& controlledBy) {
  ExpectNoCommit(name);
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.getInstance().rollbackPref("
             << "        '" << name.c_str() << "');});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_, "", javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, controlledBy, false, true);
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::StartObserving() {
  ASSERT_TRUE(content::ExecuteJavaScript(
      render_view_host_, "", "testEnv.startObserving();"));
}

void PreferencesBrowserTest::FinishObserving(std::string* observed_json) {
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      render_view_host_,
      "",
      "testEnv.finishObservingAndReply();",
      observed_json));
}

void PreferencesBrowserTest::UseDefaultTestPrefs(bool includeListPref) {
  // Boolean pref.
  types_.push_back("Boolean");
  pref_names_.push_back(prefs::kAlternateErrorPagesEnabled);
  policy_names_.push_back(policy::key::kAlternateErrorPagesEnabled);
  non_default_values_.push_back(new base::FundamentalValue(false));

  // Integer pref.
  types_.push_back("Integer");
  pref_names_.push_back(prefs::kRestoreOnStartup);
  policy_names_.push_back(policy::key::kRestoreOnStartup);
  non_default_values_.push_back(new base::FundamentalValue(4));

  // String pref.
  types_.push_back("String");
  pref_names_.push_back(prefs::kEnterpriseWebStoreName);
  policy_names_.push_back(policy::key::kEnterpriseWebStoreName);
  non_default_values_.push_back(new base::StringValue("Store"));

  // URL pref.
  types_.push_back("URL");
  pref_names_.push_back(prefs::kEnterpriseWebStoreURL);
  policy_names_.push_back(policy::key::kEnterpriseWebStoreURL);
  non_default_values_.push_back(
      new base::StringValue("http://www.google.com/"));

  // List pref.
  if (includeListPref) {
    types_.push_back("List");
    pref_names_.push_back(prefs::kURLsToRestoreOnStartup);
    policy_names_.push_back(policy::key::kRestoreOnStartupURLs);
    base::ListValue* list = new base::ListValue;
    list->Append(new base::StringValue("http://www.google.com"));
    list->Append(new base::StringValue("http://example.com"));
    non_default_values_.push_back(list);
  }

  // Retrieve default values.
  for (std::vector<std::string>::const_iterator name = pref_names_.begin();
        name != pref_names_.end(); ++name) {
    default_values_.push_back(
        pref_service_->GetDefaultPrefValue(name->c_str())->DeepCopy());
  }
}

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, FetchPrefs) {
  UseDefaultTestPrefs(true);
  std::string observed_json;

  // Verify notifications when default values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_,
                      "", false, false);

  // Verify notifications when recommended values are in effect.
  SetUserPolicies(policy_names_, non_default_values_,
                  policy::POLICY_LEVEL_RECOMMENDED);
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "recommended", false, false);

  // Verify notifications when mandatory values are in effect.
  SetUserPolicies(policy_names_, non_default_values_,
                  policy::POLICY_LEVEL_MANDATORY);
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "policy", true, false);

  // Verify notifications when user-modified values are in effect.
  ClearUserPolicies();
  SetUserValues(pref_names_, non_default_values_);
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "", false, false);
}

// Verifies that setting a user-modified pref value through the JavaScript
// Preferences class fires the correct notification in JavaScript and causes the
// change to be committed to the C++ backend.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, SetPrefs) {
  UseDefaultTestPrefs(false);

  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i)
    VerifySetPref(pref_names_[i], types_[i], non_default_values_[i], true);
}

// Verifies that clearing a user-modified pref value through the JavaScript
// Preferences class fires the correct notification in JavaScript and causes the
// change to be committed to the C++ backend.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, ClearPrefs) {
  UseDefaultTestPrefs(false);

  SetUserPolicies(policy_names_, default_values_,
                  policy::POLICY_LEVEL_RECOMMENDED);
  SetUserValues(pref_names_, non_default_values_);
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i)
    VerifyClearPref(pref_names_[i], default_values_[i], true);
}

// Verifies that when the user-modified value of a dialog pref is set and the
// change then committed through the JavaScript Preferences class, the correct
// notifications fire and a commit to the C++ backend occurs in the latter step
// only.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, DialogPrefsSetCommit) {
  UseDefaultTestPrefs(false);

  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifySetPref(pref_names_[i], types_[i], non_default_values_[i], false);
    VerifySetCommit(pref_names_[i], non_default_values_[i]);
  }
}

// Verifies that when the user-modified value of a dialog pref is set and the
// change then rolled back through the JavaScript Preferences class, the correct
// notifications fire and no commit to the C++ backend occurs.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, DialogPrefsSetRollback) {
  UseDefaultTestPrefs(false);

  // Verify behavior when default values are in effect.
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifySetPref(pref_names_[i], types_[i], non_default_values_[i], false);
    VerifyRollback(pref_names_[i], default_values_[i], "");
  }

  // Verify behavior when recommended values are in effect.
  SetUserPolicies(policy_names_, default_values_,
                  policy::POLICY_LEVEL_RECOMMENDED);
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifySetPref(pref_names_[i], types_[i], non_default_values_[i], false);
    VerifyRollback(pref_names_[i], default_values_[i], "recommended");
  }
}

// Verifies that when the user-modified value of a dialog pref is cleared and
// the change then committed through the JavaScript Preferences class, the
// correct notifications fire and a commit to the C++ backend occurs in the
// latter step only.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, DialogPrefsClearCommit) {
  UseDefaultTestPrefs(false);

  SetUserPolicies(policy_names_, default_values_,
                  policy::POLICY_LEVEL_RECOMMENDED);
  SetUserValues(pref_names_, non_default_values_);
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifyClearPref(pref_names_[i], default_values_[i], false);
    VerifyClearCommit(pref_names_[i], default_values_[i]);
  }
}

// Verifies that when the user-modified value of a dialog pref is cleared and
// the change then rolled back through the JavaScript Preferences class, the
// correct notifications fire and no commit to the C++ backend occurs.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, DialogPrefsClearRollback) {
  UseDefaultTestPrefs(false);

  SetUserPolicies(policy_names_, default_values_,
                  policy::POLICY_LEVEL_RECOMMENDED);
  SetUserValues(pref_names_, non_default_values_);
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifyClearPref(pref_names_[i], default_values_[i], false);
    VerifyRollback(pref_names_[i], non_default_values_[i], "");
  }
}

// Verifies that when preference values change in the C++ backend, the correct
// notifications fire in JavaScript.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, NotificationsOnBackendChanges) {
  UseDefaultTestPrefs(false);
  std::string observed_json;

  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));

  // Verify notifications when recommended values come into effect.
  StartObserving();
  SetUserPolicies(policy_names_, non_default_values_,
                  policy::POLICY_LEVEL_RECOMMENDED);
  FinishObserving(&observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "recommended", false, false);

  // Verify notifications when mandatory values come into effect.
  StartObserving();
  SetUserPolicies(policy_names_, non_default_values_,
                  policy::POLICY_LEVEL_MANDATORY);
  FinishObserving(&observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "policy", true, false);

  // Verify notifications when default values come into effect.
  StartObserving();
  ClearUserPolicies();
  FinishObserving(&observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_,
                      "", false, false);

  // Verify notifications when user-modified values come into effect.
  StartObserving();
  SetUserValues(pref_names_, non_default_values_);
  FinishObserving(&observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "", false, false);
}

#if defined(OS_CHROMEOS)

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript for pref values handled by the
// CoreChromeOSOptionsHandler class.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, ChromeOSDeviceFetchPrefs) {
  std::vector<base::Value*> decorated_non_default_values;
  std::string observed_json;

  // Boolean pref.
  pref_names_.push_back(chromeos::kAccountsPrefAllowGuest);
  default_values_.push_back(new base::FundamentalValue(true));
  non_default_values_.push_back(new base::FundamentalValue(false));
  decorated_non_default_values.push_back(
      non_default_values_.back()->DeepCopy());

  // String pref.
  pref_names_.push_back(chromeos::kReleaseChannel);
  default_values_.push_back(new base::StringValue(""));
  non_default_values_.push_back(new base::StringValue("stable-channel"));
  decorated_non_default_values.push_back(
      non_default_values_.back()->DeepCopy());

  // List pref.
  pref_names_.push_back(chromeos::kAccountsPrefUsers);
  default_values_.push_back(new base::ListValue);
  base::ListValue* list = new base::ListValue;
  list->Append(new base::StringValue("me@google.com"));
  list->Append(new base::StringValue("you@google.com"));
  non_default_values_.push_back(list);
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
  decorated_non_default_values.push_back(list);

  // Verify notifications when default values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_,
                      "", true, false);

  // Verify notifications when mandatory values are in effect.
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  for (size_t i = 0; i < pref_names_.size(); ++i)
    cros_settings->Set(pref_names_[i], *non_default_values_[i]);
  // FIXME(bartfab): Find a way to simulate enterprise enrollment in browser
  // tests. Only if Chrome thinks that it is enrolled will the device prefs be
  // decorated with "controlledBy: policy".
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, decorated_non_default_values,
                      "", true, false);

  STLDeleteElements(&decorated_non_default_values);
}

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript for pref values handled by the Chrome OS proxy
// settings parser.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, ChromeOSProxyFetchPrefs) {
  std::string observed_json;

  // Boolean pref.
  pref_names_.push_back(chromeos::kProxySingle);
  default_values_.push_back(new base::FundamentalValue(false));
  non_default_values_.push_back(new base::FundamentalValue(true));

  // Integer pref.
  pref_names_.push_back(chromeos::kProxySingleHttpPort);
  default_values_.push_back(new base::StringValue(""));
  non_default_values_.push_back(new base::FundamentalValue(8080));

  // String pref.
  pref_names_.push_back(chromeos::kProxySingleHttp);
  default_values_.push_back(new base::StringValue(""));
  non_default_values_.push_back(new base::StringValue("127.0.0.1"));

  // List pref.
  pref_names_.push_back(chromeos::kProxyIgnoreList);
  default_values_.push_back(new base::ListValue());
  base::ListValue* list = new base::ListValue();
  list->Append(new base::StringValue("www.google.com"));
  list->Append(new base::StringValue("example.com"));
  non_default_values_.push_back(list);

  // Verify notifications when default values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_,
                      "", false, false);

  // Verify notifications when user-modified values are in effect.
  Profile* profile = browser()->profile();
  // Do not set the Boolean pref. It will toogle automatically.
  for (size_t i = 1; i < pref_names_.size(); ++i)
    chromeos::proxy_cros_settings_parser::SetProxyPrefValue(
        profile, pref_names_[i], non_default_values_[i]->DeepCopy());
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      "", false, false);
}

#endif
