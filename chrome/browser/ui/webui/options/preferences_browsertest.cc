// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/preferences_browsertest.h"

#include <stddef.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/test/test_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

using testing::AllOf;
using testing::Mock;
using testing::Property;
using testing::Return;
using testing::_;

namespace base {

// Helper for using EXPECT_EQ() with base::Value.
bool operator==(const base::Value& first, const base::Value& second) {
  return first.Equals(&second);
}

// Helper for pretty-printing the contents of base::Value in case of failures.
void PrintTo(const base::Value& value, std::ostream* stream) {
  std::string json;
  JSONWriter::Write(value, &json);
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

PrefService* PreferencesBrowserTest::pref_service() {
  DCHECK(pref_change_registrar_);
  return pref_change_registrar_->prefs();
}

// Navigates to the settings page, causing the JavaScript pref handling code to
// load and injects JavaScript testing code.
void PreferencesBrowserTest::SetUpOnMainThread() {
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUISettingsFrameURL));
  SetUpPrefs();
}

void PreferencesBrowserTest::TearDownOnMainThread() {
  pref_change_registrar_.reset();
}

void PreferencesBrowserTest::SetUpPrefs() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  render_view_host_ = web_contents->GetRenderViewHost();
  ASSERT_TRUE(render_view_host_);

  DCHECK(!pref_change_registrar_);
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(browser()->profile()->GetPrefs());

  ASSERT_TRUE(content::ExecuteScript(render_view_host_,
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
  OnCommit(pref_service()->FindPreference(pref_name.c_str()));
}

void PreferencesBrowserTest::SetUpInProcessBrowserTestFixture() {
  // Sets up a mock policy provider for user and device policies.
  EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
      &policy_provider_);
}

void PreferencesBrowserTest::SetUserPolicies(
    const std::vector<std::string>& names,
    const std::vector<std::unique_ptr<base::Value>>& values,
    policy::PolicyLevel level) {
  policy::PolicyMap map;
  for (size_t i = 0; i < names.size(); ++i) {
    map.Set(names[i], level, policy::POLICY_SCOPE_USER,
            policy::POLICY_SOURCE_CLOUD, values[i]->CreateDeepCopy(), nullptr);
  }
  policy_provider_.UpdateChromePolicy(map);
}

void PreferencesBrowserTest::ClearUserPolicies() {
  policy::PolicyMap empty_policy_map;
  policy_provider_.UpdateChromePolicy(empty_policy_map);
}

void PreferencesBrowserTest::SetUserValues(
    const std::vector<std::string>& names,
    const std::vector<std::unique_ptr<base::Value>>& values) {
  for (size_t i = 0; i < names.size(); ++i) {
    pref_service()->Set(names[i].c_str(), *values[i]);
  }
}

void PreferencesBrowserTest::VerifyKeyValue(const base::DictionaryValue& dict,
                                            const std::string& key,
                                            const base::Value& expected) {
  const base::Value* actual = NULL;
  EXPECT_TRUE(dict.Get(key, &actual)) << "Was checking key: " << key;
  if (actual)
    EXPECT_EQ(expected, *actual) << "Was checking key: " << key;
}

void PreferencesBrowserTest::VerifyPref(
    const base::DictionaryValue* prefs,
    const std::string& name,
    const std::unique_ptr<base::Value>& value,
    const std::string& controlledBy,
    bool disabled,
    bool uncommitted) {
  const base::Value* pref = NULL;
  const base::DictionaryValue* dict = NULL;
  ASSERT_TRUE(prefs->GetWithoutPathExpansion(name, &pref));
  ASSERT_TRUE(pref->GetAsDictionary(&dict));
  VerifyKeyValue(*dict, "value", *value);
  if (!controlledBy.empty())
    VerifyKeyValue(*dict, "controlledBy", base::StringValue(controlledBy));
  else
    EXPECT_FALSE(dict->HasKey("controlledBy"));

  if (disabled)
    VerifyKeyValue(*dict, "disabled", base::Value(true));
  else if (dict->HasKey("disabled"))
    VerifyKeyValue(*dict, "disabled", base::Value(false));

  if (uncommitted)
    VerifyKeyValue(*dict, "uncommitted", base::Value(true));
  else if (dict->HasKey("uncommitted"))
    VerifyKeyValue(*dict, "uncommitted", base::Value(false));
}

void PreferencesBrowserTest::VerifyObservedPref(
    const std::string& json,
    const std::string& name,
    const std::unique_ptr<base::Value>& value,
    const std::string& controlledBy,
    bool disabled,
    bool uncommitted) {
  std::unique_ptr<base::Value> observed_value_ptr =
      base::JSONReader::Read(json);
  const base::DictionaryValue* observed_dict;
  ASSERT_TRUE(observed_value_ptr.get());
  ASSERT_TRUE(observed_value_ptr->GetAsDictionary(&observed_dict));
  VerifyPref(observed_dict, name, value, controlledBy, disabled, uncommitted);
}

void PreferencesBrowserTest::VerifyObservedPrefs(
    const std::string& json,
    const std::vector<std::string>& names,
    const std::vector<std::unique_ptr<base::Value>>& values,
    const std::string& controlledBy,
    bool disabled,
    bool uncommitted) {
  std::unique_ptr<base::Value> observed_value_ptr =
      base::JSONReader::Read(json);
  const base::DictionaryValue* observed_dict;
  ASSERT_TRUE(observed_value_ptr.get());
  ASSERT_TRUE(observed_value_ptr->GetAsDictionary(&observed_dict));
  for (size_t i = 0; i < names.size(); ++i) {
    VerifyPref(observed_dict, names[i], values[i], controlledBy, disabled,
               uncommitted);
  }
}

void PreferencesBrowserTest::ExpectNoCommit(const std::string& name) {
  pref_change_registrar_->Add(
      name.c_str(),
      base::Bind(&PreferencesBrowserTest::OnPreferenceChanged,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnCommit(Property(&PrefService::Preference::name, name)))
      .Times(0);
}

void PreferencesBrowserTest::ExpectSetCommit(
    const std::string& name,
    const std::unique_ptr<base::Value>& value) {
  pref_change_registrar_->Add(
      name.c_str(),
      base::Bind(&PreferencesBrowserTest::OnPreferenceChanged,
                 base::Unretained(this)));
  EXPECT_CALL(
      *this,
      OnCommit(AllOf(Property(&PrefService::Preference::name, name),
                     Property(&PrefService::Preference::IsUserControlled, true),
                     Property(&PrefService::Preference::GetValue,
                              EqualsValue(value.get())))));
}

void PreferencesBrowserTest::ExpectClearCommit(const std::string& name) {
  pref_change_registrar_->Add(
      name.c_str(),
      base::Bind(&PreferencesBrowserTest::OnPreferenceChanged,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnCommit(AllOf(
      Property(&PrefService::Preference::name, name),
      Property(&PrefService::Preference::IsUserControlled, false))));
}

void PreferencesBrowserTest::VerifyAndClearExpectations() {
  Mock::VerifyAndClearExpectations(this);
  pref_change_registrar_->RemoveAll();
}

void PreferencesBrowserTest::SetupJavaScriptTestEnvironment(
    const std::vector<std::string>& pref_names,
    std::string* observed_json) const {
  std::stringstream javascript;
  javascript << "var testEnv = new TestEnv();";
  for (const auto& name : pref_names) {
    javascript << "testEnv.addPref('" << name.c_str() << "');";
  }
  javascript << "testEnv.setupAndReply();";
  std::string temp_observed_json;
  if (!observed_json)
    observed_json = &temp_observed_json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_view_host_, javascript.str(), observed_json));
}

void PreferencesBrowserTest::SetPref(const std::string& name,
                                     const std::string& type,
                                     const std::unique_ptr<base::Value>& value,
                                     bool commit,
                                     std::string* observed_json) {
  std::unique_ptr<base::Value> commit_ptr(new base::Value(commit));
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "  Preferences.set" << type << "Pref("
             << "      '" << name << "',"
             << "      " << *value << ","
             << "      " << *commit_ptr << ");"
             << "});";
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_view_host_, javascript.str(), observed_json));
}

void PreferencesBrowserTest::VerifySetPref(
    const std::string& name,
    const std::string& type,
    const std::unique_ptr<base::Value>& value,
    bool commit) {
  if (commit)
    ExpectSetCommit(name, value);
  else
    ExpectNoCommit(name);
  std::string observed_json;
  SetPref(name, type, value, commit, &observed_json);
  VerifyObservedPref(observed_json, name, value, std::string(), false, !commit);
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyClearPref(
    const std::string& name,
    const std::unique_ptr<base::Value>& value,
    bool commit) {
  if (commit)
    ExpectClearCommit(name);
  else
    ExpectNoCommit(name);
  std::string commit_json;
  base::JSONWriter::Write(base::Value(commit), &commit_json);
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.clearPref("
             << "      '" << name.c_str() << "',"
             << "      " << commit_json.c_str() << ");});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_view_host_, javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, "recommended", false, !commit);
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyCommit(
    const std::string& name,
    const std::unique_ptr<base::Value>& value,
    const std::string& controlledBy) {
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.getInstance().commitPref("
             << "        '" << name.c_str() << "');});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_view_host_, javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, controlledBy, false, false);
}

void PreferencesBrowserTest::VerifySetCommit(
    const std::string& name,
    const std::unique_ptr<base::Value>& value) {
  ExpectSetCommit(name, value);
  VerifyCommit(name, value, std::string());
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyClearCommit(
    const std::string& name,
    const std::unique_ptr<base::Value>& value) {
  ExpectClearCommit(name);
  VerifyCommit(name, value, "recommended");
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::VerifyRollback(
    const std::string& name,
    const std::unique_ptr<base::Value>& value,
    const std::string& controlledBy) {
  ExpectNoCommit(name);
  std::stringstream javascript;
  javascript << "testEnv.runAndReply(function() {"
             << "    Preferences.getInstance().rollbackPref("
             << "        '" << name.c_str() << "');});";
  std::string observed_json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_view_host_, javascript.str(), &observed_json));
  VerifyObservedPref(observed_json, name, value, controlledBy, false, true);
  VerifyAndClearExpectations();
}

void PreferencesBrowserTest::StartObserving() {
  ASSERT_TRUE(content::ExecuteScript(
      render_view_host_, "testEnv.startObserving();"));
}

void PreferencesBrowserTest::FinishObserving(std::string* observed_json) {
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_view_host_,
      "testEnv.finishObservingAndReply();",
      observed_json));
}

void PreferencesBrowserTest::UseDefaultTestPrefs(bool includeListPref) {
  // Boolean pref.
  types_.push_back("Boolean");
  pref_names_.push_back(prefs::kAlternateErrorPagesEnabled);
  policy_names_.push_back(policy::key::kAlternateErrorPagesEnabled);
  non_default_values_.push_back(base::MakeUnique<base::Value>(false));

  // Integer pref.
  types_.push_back("Integer");
  pref_names_.push_back(prefs::kRestoreOnStartup);
  policy_names_.push_back(policy::key::kRestoreOnStartup);
  non_default_values_.push_back(base::MakeUnique<base::Value>(4));

  // List pref.
  if (includeListPref) {
    types_.push_back("List");
    pref_names_.push_back(prefs::kURLsToRestoreOnStartup);
    policy_names_.push_back(policy::key::kRestoreOnStartupURLs);
    auto list = base::MakeUnique<base::ListValue>();
    list->AppendString("http://www.example.com");
    list->AppendString("http://example.com");
    non_default_values_.push_back(std::move(list));
  }

  // Retrieve default values.
  for (const auto& name : pref_names_) {
    default_values_.push_back(
        pref_service()->GetDefaultPrefValue(name.c_str())->CreateDeepCopy());
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
                      std::string(), false, false);

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
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "policy",
                      true, false);

  // Verify notifications when user-modified values are in effect.
  ClearUserPolicies();
  SetUserValues(pref_names_, non_default_values_);
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      std::string(), false, false);
}

// Verifies that setting a user-modified pref value through the JavaScript
// Preferences class fires the correct notification in JavaScript and causes the
// change to be committed to the C++ backend.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, SetPrefs) {
  UseDefaultTestPrefs(false);

  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifySetPref(pref_names_[i], types_[i], non_default_values_[i], true);
  }
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
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    VerifyClearPref(pref_names_[i], default_values_[i], true);
  }
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
    VerifyRollback(pref_names_[i], default_values_[i], std::string());
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
    VerifyRollback(pref_names_[i], non_default_values_[i], std::string());
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
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "policy",
                      true, false);

  // Verify notifications when default values come into effect.
  StartObserving();
  ClearUserPolicies();
  FinishObserving(&observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_,
                      std::string(), false, false);

  // Verify notifications when user-modified values come into effect.
  StartObserving();
  SetUserValues(pref_names_, non_default_values_);
  FinishObserving(&observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_,
                      std::string(), false, false);
}

#if defined(OS_CHROMEOS)

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript for pref values handled by the
// CoreChromeOSOptionsHandler class.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest, ChromeOSDeviceFetchPrefs) {
  std::string observed_json;

  // Boolean pref.
  pref_names_.push_back(chromeos::kAccountsPrefAllowGuest);
  default_values_.push_back(base::MakeUnique<base::Value>(true));

  // String pref.
  pref_names_.push_back(chromeos::kReleaseChannel);
  default_values_.push_back(base::MakeUnique<base::StringValue>(""));

  // List pref.
  pref_names_.push_back(chromeos::kAccountsPrefUsers);
  default_values_.push_back(base::MakeUnique<base::ListValue>());

  // Verify notifications when default values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_, "owner",
                      true, false);
}

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript for non-privileged pref values handled by the
// CoreChromeOSOptionsHandler class.
IN_PROC_BROWSER_TEST_F(PreferencesBrowserTest,
                       ChromeOSDeviceFetchNonPrivilegedPrefs) {
  std::vector<std::unique_ptr<base::Value>> decorated_non_default_values;
  std::string observed_json;

  // Non-privileged string pref.
  pref_names_.push_back(chromeos::kSystemTimezone);
  default_values_.push_back(
      base::MakeUnique<base::StringValue>("America/Los_Angeles"));
  non_default_values_.push_back(
      base::MakeUnique<base::StringValue>("America/New_York"));
  decorated_non_default_values.push_back(
      non_default_values_.back()->CreateDeepCopy());

  // Verify notifications when default values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, default_values_,
                      std::string(), false, false);

  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  cros_settings->Set(pref_names_[0], *non_default_values_[0]);

  // Verify notifications when non-default values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, decorated_non_default_values,
                      std::string(), false, false);
}

class ManagedPreferencesBrowserTest : public PreferencesBrowserTest {
 protected:
  // PreferencesBrowserTest implementation:
  void SetUpInProcessBrowserTestFixture() override {
    // Set up fake install attributes.
    std::unique_ptr<chromeos::StubInstallAttributes> attributes =
        base::MakeUnique<chromeos::StubInstallAttributes>();
    attributes->SetEnterprise("example.com", "fake-id");
    policy::BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
        attributes.release());

    PreferencesBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript for pref values handled by the
// CoreChromeOSOptionsHandler class for a managed device.
IN_PROC_BROWSER_TEST_F(ManagedPreferencesBrowserTest,
                       ChromeOSDeviceFetchPrefs) {
  std::vector<std::unique_ptr<base::Value>> decorated_non_default_values;
  std::string observed_json;

  // Boolean pref.
  pref_names_.push_back(chromeos::kAccountsPrefAllowGuest);
  non_default_values_.push_back(base::MakeUnique<base::Value>(false));
  decorated_non_default_values.push_back(
      non_default_values_.back()->CreateDeepCopy());

  // String pref.
  pref_names_.push_back(chromeos::kReleaseChannel);
  non_default_values_.push_back(
      base::MakeUnique<base::StringValue>("stable-channel"));
  decorated_non_default_values.push_back(
      non_default_values_.back()->CreateDeepCopy());

  // List pref.
  pref_names_.push_back(chromeos::kAccountsPrefUsers);
  auto list = base::MakeUnique<base::ListValue>();
  list->AppendString("me@google.com");
  list->AppendString("you@google.com");
  non_default_values_.push_back(std::move(list));
  list = base::MakeUnique<base::ListValue>();
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString("username", "me@google.com");
  dict->SetString("name", "me@google.com");
  dict->SetString("email", "");
  dict->SetBoolean("owner", false);
  list->Append(std::move(dict));
  dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString("username", "you@google.com");
  dict->SetString("name", "you@google.com");
  dict->SetString("email", "");
  dict->SetBoolean("owner", false);
  list->Append(std::move(dict));
  decorated_non_default_values.push_back(std::move(list));

  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  for (size_t i = 0; i < pref_names_.size(); ++i) {
    cros_settings->Set(pref_names_[i], *non_default_values_[i]);
  }

  // Verify notifications when mandatory values are in effect.
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, decorated_non_default_values,
                      "policy", true, false);
}

// Verifies that initializing the JavaScript Preferences class fires the correct
// notifications in JavaScript for non-privileged pref values handled by the
// CoreChromeOSOptionsHandler class for a managed device.
IN_PROC_BROWSER_TEST_F(ManagedPreferencesBrowserTest,
                       ChromeOSDeviceFetchNonPrivilegedPrefs) {
  std::vector<std::unique_ptr<base::Value>> decorated_non_default_values;
  std::string observed_json;

  // Non-privileged string pref.
  pref_names_.push_back(chromeos::kSystemTimezone);
  non_default_values_.push_back(
      base::MakeUnique<base::StringValue>("America/New_York"));
  decorated_non_default_values.push_back(
      non_default_values_.back()->CreateDeepCopy());

  // Verify notifications when mandatory values are in effect.
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  cros_settings->Set(pref_names_[0], *non_default_values_[0]);

  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, decorated_non_default_values,
                      std::string(), false, false);
}

namespace {

const char* kUserProfilePath = "user_profile";

}  // namespace

class ProxyPreferencesBrowserTest : public PreferencesBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SetupNetworkEnvironment();
    content::RunAllPendingInMessageLoop();

    std::unique_ptr<base::DictionaryValue> proxy_config_dict(
        ProxyConfigDictionary::CreateFixedServers("127.0.0.1:8080",
                                                  "*.google.com, 1.2.3.4:22"));

    ProxyConfigDictionary proxy_config(proxy_config_dict.get());

    const chromeos::NetworkState* network = GetDefaultNetwork();
    ASSERT_TRUE(network);
    chromeos::proxy_config::SetProxyConfigForNetwork(proxy_config, *network);

    std::string url = base::StringPrintf("%s?network=%s",
                                         chrome::kChromeUIProxySettingsURL,
                                         network->guid().c_str());

    ui_test_utils::NavigateToURL(browser(), GURL(url));
    SetUpPrefs();
  }

 protected:
  void SetupNetworkEnvironment() {
    chromeos::ShillProfileClient::TestInterface* profile_test =
        chromeos::DBusThreadManager::Get()->GetShillProfileClient()
            ->GetTestInterface();
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::DBusThreadManager::Get()->GetShillServiceClient()
            ->GetTestInterface();

    profile_test->AddProfile(kUserProfilePath, "user");

    service_test->ClearServices();
    service_test->AddService("stub_ethernet",
                             "stub_ethernet_guid",
                             "eth0",
                             shill::kTypeEthernet,
                             shill::kStateOnline,
                             true /* add_to_visible */ );
    service_test->SetServiceProperty("stub_ethernet",
                                     shill::kProfileProperty,
                                     base::StringValue(kUserProfilePath));
    profile_test->AddService(kUserProfilePath, "stub_wifi2");
  }

  void SetONCPolicy(const char* policy_name, policy::PolicyScope scope) {
    std::string onc_policy =
        "{ \"NetworkConfigurations\": ["
        "    { \"GUID\": \"stub_ethernet_guid\","
        "      \"Type\": \"Ethernet\","
        "      \"Name\": \"My Ethernet\","
        "      \"Ethernet\": {"
        "        \"Authentication\": \"None\" },"
        "      \"ProxySettings\": {"
        "        \"PAC\": \"http://domain.com/x\","
        "        \"Type\": \"PAC\" }"
        "    }"
        "  ],"
        "  \"Type\": \"UnencryptedConfiguration\""
        "}";

    policy::PolicyMap map;
    map.Set(policy_name, policy::POLICY_LEVEL_MANDATORY, scope,
            policy::POLICY_SOURCE_CLOUD,
            base::MakeUnique<base::StringValue>(onc_policy), nullptr);
    policy_provider_.UpdateChromePolicy(map);

    content::RunAllPendingInMessageLoop();
  }

  const chromeos::NetworkState* GetDefaultNetwork() {
    chromeos::NetworkStateHandler* handler =
        chromeos::NetworkHandler::Get()->network_state_handler();
    return handler->DefaultNetwork();
  }

  void SetProxyPref(const std::string& name, const base::Value& value) {
    std::string type;
    switch (value.GetType()) {
      case base::Value::Type::BOOLEAN:
        type = "Boolean";
        break;
      case base::Value::Type::INTEGER:
        type = "Integer";
        break;
      case base::Value::Type::STRING:
        type = "String";
        break;
      default:
        ASSERT_TRUE(false);
    }

    std::string observed_json;
    SetPref(name, type, value.CreateDeepCopy(), true, &observed_json);
  }

  void VerifyCurrentProxyServer(const std::string& expected_server,
                                onc::ONCSource expected_source) {
    const chromeos::NetworkState* network = GetDefaultNetwork();
    ASSERT_TRUE(network);
    onc::ONCSource actual_source;
    std::unique_ptr<ProxyConfigDictionary> proxy_dict =
        chromeos::proxy_config::GetProxyConfigForNetwork(
            g_browser_process->local_state(), pref_service(), *network,
            &actual_source);
    ASSERT_TRUE(proxy_dict);
    std::string actual_proxy_server;
    EXPECT_TRUE(proxy_dict->GetProxyServer(&actual_proxy_server));
    EXPECT_EQ(expected_server, actual_proxy_server);
    EXPECT_EQ(expected_source, actual_source);
  }
};

// Verifies that proxy settings are correctly pushed to JavaScript during
// initialization of the proxy settings page.
IN_PROC_BROWSER_TEST_F(ProxyPreferencesBrowserTest, ChromeOSInitializeProxy) {
  // Boolean pref.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxySingle);
  non_default_values_.push_back(base::MakeUnique<base::Value>(true));

  // Integer prefs.
  pref_names_.push_back(
      chromeos::proxy_cros_settings_parser::kProxySingleHttpPort);
  non_default_values_.push_back(base::MakeUnique<base::Value>(8080));

  // String pref.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxySingleHttp);
  non_default_values_.push_back(
      base::MakeUnique<base::StringValue>("127.0.0.1"));

  // List pref.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxyIgnoreList);
  auto list = base::MakeUnique<base::ListValue>();
  list->AppendString("*.google.com");
  list->AppendString("1.2.3.4:22");
  non_default_values_.push_back(std::move(list));

  // Verify that no policy is presented to the UI. This must be verified on the
  // kProxyType and the kUseSharedProxies prefs.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxyType);
  non_default_values_.push_back(base::MakeUnique<base::Value>(2));

  pref_names_.push_back(proxy_config::prefs::kUseSharedProxies);
  non_default_values_.push_back(base::MakeUnique<base::Value>(false));

  std::string observed_json;
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "",
                      false, false);
}

IN_PROC_BROWSER_TEST_F(ProxyPreferencesBrowserTest, ONCPolicy) {
  SetONCPolicy(policy::key::kOpenNetworkConfiguration,
               policy::POLICY_SCOPE_USER);

  // Verify that per-network policy is presented to the UI. This must be
  // verified on the kProxyType.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxyType);
  non_default_values_.push_back(base::MakeUnique<base::Value>(3));

  std::string observed_json;
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "policy",
                      true, false);

  // Verify that 'use-shared-proxies' is not affected by per-network policy.
  pref_names_.clear();
  non_default_values_.clear();
  pref_names_.push_back(proxy_config::prefs::kUseSharedProxies);
  non_default_values_.push_back(base::MakeUnique<base::Value>(false));

  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "",
                      false, false);
}

IN_PROC_BROWSER_TEST_F(ProxyPreferencesBrowserTest, DeviceONCPolicy) {
  SetONCPolicy(policy::key::kDeviceOpenNetworkConfiguration,
               policy::POLICY_SCOPE_MACHINE);

  // Verify that the policy is presented to the UI. This verification must be
  // done on the kProxyType pref.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxyType);
  non_default_values_.push_back(base::MakeUnique<base::Value>(3));

  std::string observed_json;
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "policy",
                      true, false);

  // Verify that 'use-shared-proxies' is not affected by per-network policy.
  pref_names_.clear();
  non_default_values_.clear();
  pref_names_.push_back(proxy_config::prefs::kUseSharedProxies);
  non_default_values_.push_back(base::MakeUnique<base::Value>(false));

  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "",
                      false, false);
}

IN_PROC_BROWSER_TEST_F(ProxyPreferencesBrowserTest, UserProxyPolicy) {
  policy_names_.push_back(policy::key::kProxyMode);
  default_values_.push_back(base::MakeUnique<base::StringValue>(
      ProxyPrefs::kAutoDetectProxyModeName));
  SetUserPolicies(policy_names_, default_values_,
                  policy::POLICY_LEVEL_MANDATORY);
  content::RunAllPendingInMessageLoop();

  // Verify that the policy is presented to the UI. This verification must be
  // done on the kProxyType pref.
  pref_names_.push_back(chromeos::proxy_cros_settings_parser::kProxyType);
  non_default_values_.push_back(base::MakeUnique<base::Value>(3));

  // Verify that 'use-shared-proxies' is controlled by the policy.
  pref_names_.push_back(proxy_config::prefs::kUseSharedProxies);
  non_default_values_.push_back(base::MakeUnique<base::Value>(false));

  std::string observed_json;
  SetupJavaScriptTestEnvironment(pref_names_, &observed_json);
  VerifyObservedPrefs(observed_json, pref_names_, non_default_values_, "policy",
                      true, false);
}

// Verifies that modifications to the proxy settings are correctly pushed from
// JavaScript to the ProxyConfig property stored in the network configuration.
IN_PROC_BROWSER_TEST_F(ProxyPreferencesBrowserTest, ChromeOSSetProxy) {
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));

  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxySingleHttpPort,
               base::Value(123));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxySingleHttp,
               base::StringValue("www.adomain.xy"));

  VerifyCurrentProxyServer("www.adomain.xy:123",
                           onc::ONC_SOURCE_NONE);
}

// Verify that default proxy ports are used and that ports can be updated
// without affecting the previously set hosts.
IN_PROC_BROWSER_TEST_F(ProxyPreferencesBrowserTest, ChromeOSProxyDefaultPorts) {
  ASSERT_NO_FATAL_FAILURE(SetupJavaScriptTestEnvironment(pref_names_, NULL));

  // Set to manual, per scheme proxy.
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxySingle,
               base::Value(false));

  // Set hosts but no ports.
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxyHttpUrl,
               base::StringValue("a.com"));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxyHttpsUrl,
               base::StringValue("4.3.2.1"));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxyFtpUrl,
               base::StringValue("c.com"));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxySocks,
               base::StringValue("d.com"));

  // Verify default ports.
  VerifyCurrentProxyServer(
      "http=a.com:80;https=4.3.2.1:80;ftp=c.com:80;socks=socks4://d.com:1080",
      onc::ONC_SOURCE_NONE);

  // Set and verify the ports.
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxyHttpPort,
               base::Value(1));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxyHttpsPort,
               base::Value(2));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxyFtpPort,
               base::Value(3));
  SetProxyPref(chromeos::proxy_cros_settings_parser::kProxySocksPort,
               base::Value(4));

  VerifyCurrentProxyServer(
      "http=a.com:1;https=4.3.2.1:2;ftp=c.com:3;socks=socks4://d.com:4",
      onc::ONC_SOURCE_NONE);
}

#endif
