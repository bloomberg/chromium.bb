// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace policy {

namespace {

const char* kSettingsPages[] = {
  "chrome://settings-frame",
  "chrome://settings-frame/searchEngines",
  "chrome://settings-frame/passwords",
  "chrome://settings-frame/autofill",
  "chrome://settings-frame/content",
  "chrome://settings-frame/homePageOverlay",
  "chrome://settings-frame/languages",
#if defined(OS_CHROMEOS)
  "chrome://settings-frame/accounts",
#endif
};

// Contains the testing details for a single policy, loaded from
// chrome/test/data/policy/policy_test_cases.json.
class PolicyTestCase {
 public:
  explicit PolicyTestCase(const std::string& name)
      : name_(name),
        is_local_state_(false),
        official_only_(false) {}
  ~PolicyTestCase() {}

  const std::string& name() const { return name_; }

  void set_pref(const std::string& pref) { pref_ = pref; }
  const std::string& pref() const { return pref_; }
  const char* pref_name() const { return pref_.c_str(); }

  const PolicyMap& test_policy() const { return test_policy_; }
  void set_test_policy(const PolicyMap& policy) {
    test_policy_.CopyFrom(policy);
  }

  const std::vector<GURL>& settings_pages() const { return settings_pages_; }
  void AddSettingsPage(const GURL& url) { settings_pages_.push_back(url); }

  bool IsOsSupported() const {
#if defined(OS_WIN)
    const std::string os("win");
#elif defined(OS_MACOSX)
    const std::string os("mac");
#elif defined(OS_CHROMEOS)
    const std::string os("chromeos");
#elif defined(OS_LINUX)
    const std::string os("linux");
#else
#error "Unknown platform"
#endif
    return std::find(supported_os_.begin(), supported_os_.end(), os) !=
        supported_os_.end();
  }

  void AddSupportedOs(const std::string& os) { supported_os_.push_back(os); }

  bool is_local_state() const { return is_local_state_; }
  void set_local_state(bool flag) { is_local_state_ = flag; }

  bool is_official_only() const { return official_only_; }
  void set_official_only(bool flag) { official_only_ = flag; }

  bool IsSupported() const {
#if !defined(OFFICIAL_BUILD)
    if (is_official_only())
      return false;
#endif
    return IsOsSupported();
  }

 private:
  std::string name_;
  std::string pref_;
  PolicyMap test_policy_;
  std::vector<GURL> settings_pages_;
  std::vector<std::string> supported_os_;
  bool is_local_state_;
  bool official_only_;

  DISALLOW_COPY_AND_ASSIGN(PolicyTestCase);
};

// Parses all the test cases and makes then available in a map.
class TestCases {
 public:
  typedef std::map<std::string, PolicyTestCase*> TestCaseMap;
  typedef TestCaseMap::const_iterator iterator;

  TestCases() {
    test_cases_ = new std::map<std::string, PolicyTestCase*>();

    FilePath path = ui_test_utils::GetTestFilePath(
        FilePath(FILE_PATH_LITERAL("policy")),
        FilePath(FILE_PATH_LITERAL("policy_test_cases.json")));
    std::string json;
    if (!file_util::ReadFileToString(path, &json)) {
      ADD_FAILURE();
      return;
    }
    int error_code = -1;
    std::string error_string;
    base::DictionaryValue* dict = NULL;
    scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
        json, base::JSON_PARSE_RFC, &error_code, &error_string));
    if (!value.get() || !value->GetAsDictionary(&dict)) {
      ADD_FAILURE() << "Error parsing policy_test_cases.json: " << error_string;
      return;
    }
    const PolicyDefinitionList* list = GetChromePolicyDefinitionList();
    for (const PolicyDefinitionList::Entry* policy = list->begin;
         policy != list->end; ++policy) {
      PolicyTestCase* test_case = GetTestCase(dict, policy->name);
      if (test_case)
        (*test_cases_)[policy->name] = test_case;
    }
  }

  ~TestCases() {
    STLDeleteValues(test_cases_);
    delete test_cases_;
  }

  const PolicyTestCase* Get(const std::string& name) {
    iterator it = test_cases_->find(name);
    return it == end() ? NULL : it->second;
  }

  const TestCaseMap& map() const { return *test_cases_; }
  iterator begin() const { return test_cases_->begin(); }
  iterator end() const { return test_cases_->end(); }

 private:
  PolicyTestCase* GetTestCase(const base::DictionaryValue* tests,
                              const std::string& name) {
    const base::DictionaryValue* dict = NULL;
    if (!tests->GetDictionary(name, &dict))
      return NULL;
    PolicyTestCase* test_case = new PolicyTestCase(name);
    std::string pref;
    if (dict->GetString("pref", &pref))
      test_case->set_pref(pref);
    const base::DictionaryValue* policy_dict = NULL;
    if (dict->GetDictionary("test_policy", &policy_dict)) {
      PolicyMap policies;
      policies.LoadFrom(policy_dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
      test_case->set_test_policy(policies);
    }
    const base::ListValue* settings_pages = NULL;
    if (dict->GetList("settings_pages", &settings_pages)) {
      for (size_t i = 0; i < settings_pages->GetSize(); ++i) {
        std::string page;
        if (settings_pages->GetString(i, &page))
          test_case->AddSettingsPage(GURL(page));
      }
    }
    const base::ListValue* supported_os = NULL;
    if (dict->GetList("os", &supported_os)) {
      for (size_t i = 0; i < supported_os->GetSize(); ++i) {
        std::string os;
        if (supported_os->GetString(i, &os))
          test_case->AddSupportedOs(os);
      }
    }
    bool flag = false;
    if (dict->GetBoolean("local_state", &flag))
      test_case->set_local_state(flag);
    if (dict->GetBoolean("official_only", &flag))
      test_case->set_official_only(flag);
    return test_case;
  }

  TestCaseMap* test_cases_;

  DISALLOW_COPY_AND_ASSIGN(TestCases);
};

bool IsBannerVisible(Browser* browser) {
  content::WebContents* contents = chrome::GetActiveWebContents(browser);
  bool result = false;
  EXPECT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(),
      std::wstring(),
      L"var visible = false;"
      L"var banners = document.querySelectorAll('.page-banner');"
      L"for (var i = 0; i < banners.length; i++) {"
      L"  if (banners[i].parentElement.id == 'templates')"
      L"    continue;"
      L"  if (window.getComputedStyle(banners[i]).display != 'none')"
      L"    visible = true;"
      L"}"
      L"domAutomationController.send(visible);",
      &result));
  return result;
}

}  // namespace

// A class of tests parameterized by a settings page URL.
class PolicyPrefsSettingsBannerTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {};

// Base class for tests that change policies.
class PolicyBaseTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  TestCases test_cases_;
  MockConfigurationPolicyProvider provider_;
};

// A class of tests that change policy and don't need parameters.
class PolicyPrefsBannerTest : public PolicyBaseTest {};

// A class of tests that change policy and are parameterized with a policy
// definition.
class PolicyPrefsTest
    : public PolicyBaseTest,
      public testing::WithParamInterface<PolicyDefinitionList::Entry> {};

TEST(PolicyPrefsTest, AllPoliciesHaveATestCase) {
  // Verifies that all known policies have a test case in the JSON file.
  // This test fails when a policy is added to
  // chrome/app/policy/policy_templates.json but a test case is not added to
  // chrome/test/data/policy/policy_test_cases.json.
  TestCases test_cases;
  const PolicyDefinitionList* list = GetChromePolicyDefinitionList();
  for (const PolicyDefinitionList::Entry* policy = list->begin;
       policy != list->end; ++policy) {
    EXPECT_TRUE(ContainsKey(test_cases.map(), policy->name))
        << "Missing policy test case for: " << policy->name;
  }
}

IN_PROC_BROWSER_TEST_P(PolicyPrefsSettingsBannerTest, NoPoliciesNoBanner) {
  // Verifies that the banner isn't shown in the settings UI when no policies
  // are set.
  ui_test_utils::NavigateToURL(browser(), GURL(GetParam()));
  EXPECT_FALSE(IsBannerVisible(browser()));
}

INSTANTIATE_TEST_CASE_P(PolicyPrefsSettingsBannerTestInstance,
                        PolicyPrefsSettingsBannerTest,
                        testing::ValuesIn(kSettingsPages));

IN_PROC_BROWSER_TEST_F(PolicyPrefsBannerTest, TogglePolicyTogglesBanner) {
  // Verifies that the banner appears and disappears as policies are added and
  // removed.
  // |test_case| is just a particular policy that should trigger the banner
  // on the main settings page.
  const PolicyTestCase* test_case = test_cases_.Get("ShowHomeButton");
  ASSERT_TRUE(test_case);
  // No banner by default.
  ui_test_utils::NavigateToURL(browser(), GURL(kSettingsPages[0]));
  EXPECT_FALSE(IsBannerVisible(browser()));
  // Adding a policy makes the banner show up.
  provider_.UpdateChromePolicy(test_case->test_policy());
  EXPECT_TRUE(IsBannerVisible(browser()));
  // And removing it makes the banner go away.
  const PolicyMap kNoPolicies;
  provider_.UpdateChromePolicy(kNoPolicies);
  EXPECT_FALSE(IsBannerVisible(browser()));
  // Do it again, just in case.
  provider_.UpdateChromePolicy(test_case->test_policy());
  EXPECT_TRUE(IsBannerVisible(browser()));
  provider_.UpdateChromePolicy(kNoPolicies);
  EXPECT_FALSE(IsBannerVisible(browser()));
}

IN_PROC_BROWSER_TEST_P(PolicyPrefsTest, PolicyToPrefsMapping) {
  // Verifies that policies make their corresponding preferences become managed,
  // and that the user can't override that setting.
  const PolicyTestCase* test_case = test_cases_.Get(GetParam().name);
  ASSERT_TRUE(test_case);
  if (!test_case->IsSupported() || test_case->pref().empty())
    return;
  LOG(INFO) << "Testing policy: " << test_case->name();

  PrefService* prefs = test_case->is_local_state() ?
      g_browser_process->local_state() : browser()->profile()->GetPrefs();
  // The preference must have been registered.
  const PrefService::Preference* pref =
      prefs->FindPreference(test_case->pref_name());
  ASSERT_TRUE(pref);
  prefs->ClearPref(test_case->pref_name());

  // Verify that setting the policy overrides the pref.
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_TRUE(pref->IsUserModifiable());
  EXPECT_FALSE(pref->IsUserControlled());
  EXPECT_FALSE(pref->IsManaged());

  provider_.UpdateChromePolicy(test_case->test_policy());
  EXPECT_FALSE(pref->IsDefaultValue());
  EXPECT_FALSE(pref->IsUserModifiable());
  EXPECT_FALSE(pref->IsUserControlled());
  EXPECT_TRUE(pref->IsManaged());
}

IN_PROC_BROWSER_TEST_P(PolicyPrefsTest, CheckAllPoliciesThatShowTheBanner) {
  // Verifies that the banner appears for each policy that affects a control
  // in the settings UI.
  const PolicyTestCase* test_case = test_cases_.Get(GetParam().name);
  ASSERT_TRUE(test_case);
  if (!test_case->IsSupported() || test_case->settings_pages().empty())
    return;
  LOG(INFO) << "Testing policy: " << test_case->name();

  const std::vector<GURL>& pages = test_case->settings_pages();
  for (size_t i = 0; i < pages.size(); ++i) {
    ui_test_utils::NavigateToURL(browser(), pages[i]);
    EXPECT_FALSE(IsBannerVisible(browser()));
    provider_.UpdateChromePolicy(test_case->test_policy());
    EXPECT_TRUE(IsBannerVisible(browser()));
    const PolicyMap kNoPolicies;
    provider_.UpdateChromePolicy(kNoPolicies);
    EXPECT_FALSE(IsBannerVisible(browser()));
  }
}

INSTANTIATE_TEST_CASE_P(
    PolicyPrefsTestInstance,
    PolicyPrefsTest,
    testing::ValuesIn(GetChromePolicyDefinitionList()->begin,
                      GetChromePolicyDefinitionList()->end));

}  // namespace policy
