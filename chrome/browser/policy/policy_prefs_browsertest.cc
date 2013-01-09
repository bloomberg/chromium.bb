// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
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

const char kMainSettingsPage[] = "chrome://settings-frame";

const char kCrosSettingsPrefix[] = "cros.";

// Contains the details of a single test case verifying that the controlled
// setting indicators for a pref affected by a policy work correctly. This is
// part of the data loaded from chrome/test/data/policy/policy_test_cases.json.
class IndicatorTestCase {
 public:
  IndicatorTestCase(const base::DictionaryValue& policy,
                    const std::string& value,
                    bool readonly)
      : policy_(policy.DeepCopy()), value_(value), readonly_(readonly) {}
  ~IndicatorTestCase() {}

  const base::DictionaryValue& policy() const { return *policy_; }

  const std::string& value() const { return value_; }

  bool readonly() const { return readonly_; }

 private:
  scoped_ptr<base::DictionaryValue> policy_;
  std::string value_;
  bool readonly_;

  DISALLOW_COPY_AND_ASSIGN(IndicatorTestCase);
};

// Contains the testing details for a single pref affected by a policy. This is
// part of the data loaded from chrome/test/data/policy/policy_test_cases.json.
class PrefMapping {
 public:
  PrefMapping(const std::string& pref,
              bool is_local_state,
              const std::string& indicator_test_setup_js,
              const std::string& indicator_selector)
      : pref_(pref),
        is_local_state_(is_local_state),
        indicator_test_setup_js_(indicator_test_setup_js),
        indicator_selector_(indicator_selector) {
  }
  ~PrefMapping() {}

  const std::string& pref() const { return pref_; }

  bool is_local_state() const { return is_local_state_; }

  const std::string& indicator_test_setup_js() const {
    return indicator_test_setup_js_;
  }

  const std::string& indicator_selector() const {
    return indicator_selector_;
  }

  const ScopedVector<IndicatorTestCase>& indicator_test_cases() const {
    return indicator_test_cases_;
  }
  void AddIndicatorTestCase(IndicatorTestCase* test_case) {
    indicator_test_cases_.push_back(test_case);
  }

 private:
  std::string pref_;
  bool is_local_state_;
  std::string indicator_test_setup_js_;
  std::string indicator_selector_;
  ScopedVector<IndicatorTestCase> indicator_test_cases_;

  DISALLOW_COPY_AND_ASSIGN(PrefMapping);
};

// Contains the testing details for a single policy. This is part of the data
// loaded from chrome/test/data/policy/policy_test_cases.json.
class PolicyTestCase {
 public:
  PolicyTestCase(const std::string& name,
                 bool is_official_only,
                 bool can_be_recommended)
      : name_(name),
        is_official_only_(is_official_only),
        can_be_recommended_(can_be_recommended) {}
  ~PolicyTestCase() {}

  const std::string& name() const { return name_; }

  bool is_official_only() const { return is_official_only_; }

  bool can_be_recommended() const { return can_be_recommended_; }

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

  bool IsSupported() const {
#if !defined(OFFICIAL_BUILD)
    if (is_official_only())
      return false;
#endif
    return IsOsSupported();
  }

  const PolicyMap& test_policy() const { return test_policy_; }
  void SetTestPolicy(const PolicyMap& policy) {
    test_policy_.CopyFrom(policy);
  }

  const ScopedVector<PrefMapping>& pref_mappings() const {
    return pref_mappings_;
  }
  void AddPrefMapping(PrefMapping* pref_mapping) {
    pref_mappings_.push_back(pref_mapping);
  }

 private:
  std::string name_;
  bool is_official_only_;
  bool can_be_recommended_;
  std::vector<std::string> supported_os_;
  PolicyMap test_policy_;
  ScopedVector<PrefMapping> pref_mappings_;

  DISALLOW_COPY_AND_ASSIGN(PolicyTestCase);
};

// Parses all policy test cases and makes then available in a map.
class PolicyTestCases {
 public:
  typedef std::map<std::string, PolicyTestCase*> PolicyTestCaseMap;
  typedef PolicyTestCaseMap::const_iterator iterator;

  PolicyTestCases() {
    policy_test_cases_ = new std::map<std::string, PolicyTestCase*>();

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
      PolicyTestCase* policy_test_case = GetPolicyTestCase(dict, policy->name);
      if (policy_test_case)
        (*policy_test_cases_)[policy->name] = policy_test_case;
    }
  }

  ~PolicyTestCases() {
    STLDeleteValues(policy_test_cases_);
    delete policy_test_cases_;
  }

  const PolicyTestCase* Get(const std::string& name) {
    iterator it = policy_test_cases_->find(name);
    return it == end() ? NULL : it->second;
  }

  const PolicyTestCaseMap& map() const { return *policy_test_cases_; }
  iterator begin() const { return policy_test_cases_->begin(); }
  iterator end() const { return policy_test_cases_->end(); }

 private:
  PolicyTestCase* GetPolicyTestCase(const base::DictionaryValue* tests,
                                    const std::string& name) {
    const base::DictionaryValue* policy_test_dict = NULL;
    if (!tests->GetDictionary(name, &policy_test_dict))
      return NULL;
    bool is_official_only = false;
    policy_test_dict->GetBoolean("official_only", &is_official_only);
    bool can_be_recommended = false;
    policy_test_dict->GetBoolean("can_be_recommended", &can_be_recommended);
    PolicyTestCase* policy_test_case =
        new PolicyTestCase(name, is_official_only, can_be_recommended);
    const base::ListValue* os_list = NULL;
    if (policy_test_dict->GetList("os", &os_list)) {
      for (size_t i = 0; i < os_list->GetSize(); ++i) {
        std::string os;
        if (os_list->GetString(i, &os))
          policy_test_case->AddSupportedOs(os);
      }
    }
    const base::DictionaryValue* policy_dict = NULL;
    if (policy_test_dict->GetDictionary("test_policy", &policy_dict)) {
      PolicyMap policy;
      policy.LoadFrom(policy_dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
      policy_test_case->SetTestPolicy(policy);
    }
    const base::ListValue* pref_mappings = NULL;
    if (policy_test_dict->GetList("pref_mappings", &pref_mappings)) {
      for (size_t i = 0; i < pref_mappings->GetSize(); ++i) {
        const base::DictionaryValue* pref_mapping_dict = NULL;
        std::string pref;
        if (!pref_mappings->GetDictionary(i, &pref_mapping_dict) ||
            !pref_mapping_dict->GetString("pref", &pref)) {
          ADD_FAILURE() << "Malformed pref_mappings entry in "
                        << "policy_test_cases.json.";
          continue;
        }
        bool is_local_state = false;
        pref_mapping_dict->GetBoolean("local_state", &is_local_state);
        std::string indicator_test_setup_js;
        pref_mapping_dict->GetString("indicator_test_setup_js",
                                     &indicator_test_setup_js);
        std::string indicator_selector;
        pref_mapping_dict->GetString("indicator_selector", &indicator_selector);
        PrefMapping* pref_mapping = new PrefMapping(
            pref, is_local_state, indicator_test_setup_js, indicator_selector);
        const base::ListValue* indicator_tests = NULL;
        if (pref_mapping_dict->GetList("indicator_tests", &indicator_tests)) {
          for (size_t i = 0; i < indicator_tests->GetSize(); ++i) {
            const base::DictionaryValue* indicator_test_dict = NULL;
            const base::DictionaryValue* policy = NULL;
            if (!indicator_tests->GetDictionary(i, &indicator_test_dict) ||
                !indicator_test_dict->GetDictionary("policy", &policy)) {
              ADD_FAILURE() << "Malformed indicator_tests entry in "
                            << "policy_test_cases.json.";
              continue;
            }
            std::string value;
            indicator_test_dict->GetString("value", &value);
            bool readonly = false;
            indicator_test_dict->GetBoolean("readonly", &readonly);
            pref_mapping->AddIndicatorTestCase(
                new IndicatorTestCase(*policy, value, readonly));
          }
        }
        policy_test_case->AddPrefMapping(pref_mapping);
      }
    }
    return policy_test_case;
  }

  PolicyTestCaseMap* policy_test_cases_;

  DISALLOW_COPY_AND_ASSIGN(PolicyTestCases);
};

void VerifyControlledSettingIndicators(Browser* browser,
                                       const std::string& selector,
                                       const std::string& value,
                                       const std::string& controlled_by,
                                       bool readonly) {
  std::stringstream javascript;
  javascript << "var nodes = document.querySelectorAll("
             << "    'span.controlled-setting-indicator"
             <<          selector.c_str() << "');"
             << "var indicators = [];"
             << "for (var i = 0; i < nodes.length; i++) {"
             << "  var node = nodes[i];"
             << "  var indicator = {};"
             << "  indicator.value = node.value || '';"
             << "  indicator.controlledBy = node.controlledBy || '';"
             << "  indicator.readOnly = node.readOnly || false;"
             << "  indicator.visible ="
             << "      window.getComputedStyle(node).display != 'none';"
             << "  indicators.push(indicator)"
             << "}"
             << "domAutomationController.send(JSON.stringify(indicators));";
  content::WebContents* contents = chrome::GetActiveWebContents(browser);
  std::string json;
  // Retrieve the state of all controlled setting indicators matching the
  // |selector| as JSON.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents, javascript.str(),
                                                     &json));
  scoped_ptr<base::Value> value_ptr(base::JSONReader::Read(json));
  const base::ListValue* indicators = NULL;
  ASSERT_TRUE(value_ptr.get());
  ASSERT_TRUE(value_ptr->GetAsList(&indicators));
  // Verify that controlled setting indicators representing |value| are visible
  // and have the correct state while those not representing |value| are
  // invisible.
  if (!controlled_by.empty()) {
    EXPECT_GT(indicators->GetSize(), 0u)
        << "Expected to find at least one controlled setting indicator.";
  }
  bool have_visible_indicators = false;
  for (base::ListValue::const_iterator indicator = indicators->begin();
       indicator != indicators->end(); ++indicator) {
    const base::DictionaryValue* properties = NULL;
    ASSERT_TRUE((*indicator)->GetAsDictionary(&properties));
    std::string indicator_value;
    std::string indicator_controlled_by;
    bool indicator_readonly;
    bool indicator_visible;
    EXPECT_TRUE(properties->GetString("value", &indicator_value));
    EXPECT_TRUE(properties->GetString("controlledBy",
                                      &indicator_controlled_by));
    EXPECT_TRUE(properties->GetBoolean("readOnly", &indicator_readonly));
    EXPECT_TRUE(properties->GetBoolean("visible", &indicator_visible));
    if (!controlled_by.empty() && (indicator_value == value)) {
      EXPECT_EQ(controlled_by, indicator_controlled_by);
      EXPECT_EQ(readonly, indicator_readonly);
      EXPECT_TRUE(indicator_visible);
      have_visible_indicators = true;
    } else {
      EXPECT_FALSE(indicator_visible);
    }
  }
  if (!controlled_by.empty()) {
    EXPECT_TRUE(have_visible_indicators)
        << "Expected to find at least one visible controlled setting "
        << "indicator.";
  }
}

}  // namespace

// Base class for tests that change policy and are parameterized with a policy
// definition.
class PolicyPrefsTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PolicyDefinitionList::Entry> {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ui_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  PolicyTestCases policy_test_cases_;
  MockConfigurationPolicyProvider provider_;
};

TEST(PolicyPrefsTestCoverageTest, AllPoliciesHaveATestCase) {
  // Verifies that all known policies have a test case in the JSON file.
  // This test fails when a policy is added to
  // chrome/app/policy/policy_templates.json but a test case is not added to
  // chrome/test/data/policy/policy_test_cases.json.
  PolicyTestCases policy_test_cases;
  const PolicyDefinitionList* list = GetChromePolicyDefinitionList();
  for (const PolicyDefinitionList::Entry* policy = list->begin;
       policy != list->end; ++policy) {
    EXPECT_TRUE(ContainsKey(policy_test_cases.map(), policy->name))
        << "Missing policy test case for: " << policy->name;
  }
}

IN_PROC_BROWSER_TEST_P(PolicyPrefsTest, PolicyToPrefsMapping) {
  // Verifies that policies make their corresponding preferences become managed,
  // and that the user can't override that setting.
  const PolicyTestCase* test_case = policy_test_cases_.Get(GetParam().name);
  ASSERT_TRUE(test_case);
  const ScopedVector<PrefMapping>& pref_mappings = test_case->pref_mappings();
  if (!test_case->IsSupported() || pref_mappings.empty())
    return;
  LOG(INFO) << "Testing policy: " << test_case->name();

  for (ScopedVector<PrefMapping>::const_iterator
           pref_mapping = pref_mappings.begin();
       pref_mapping != pref_mappings.end();
       ++pref_mapping) {
    // Skip Chrome OS preferences that use a different backend and cannot be
    // retrieved through the prefs mechanism.
    if (StartsWithASCII((*pref_mapping)->pref(), kCrosSettingsPrefix, true))
      continue;

    PrefService* local_state = g_browser_process->local_state();
    PrefService* user_prefs = browser()->profile()->GetPrefs();
    PrefService* prefs = (*pref_mapping)->is_local_state() ?
        local_state : user_prefs;
    // The preference must have been registered.
    const PrefService::Preference* pref =
        prefs->FindPreference((*pref_mapping)->pref().c_str());
    ASSERT_TRUE(pref);
    prefs->ClearPref((*pref_mapping)->pref().c_str());

    // Verify that setting the policy overrides the pref.
    const PolicyMap kNoPolicies;
    UpdateProviderPolicy(kNoPolicies);
    EXPECT_TRUE(pref->IsDefaultValue());
    EXPECT_TRUE(pref->IsUserModifiable());
    EXPECT_FALSE(pref->IsUserControlled());
    EXPECT_FALSE(pref->IsManaged());

    UpdateProviderPolicy(test_case->test_policy());
    EXPECT_FALSE(pref->IsDefaultValue());
    EXPECT_FALSE(pref->IsUserModifiable());
    EXPECT_FALSE(pref->IsUserControlled());
    EXPECT_TRUE(pref->IsManaged());
  }
}

IN_PROC_BROWSER_TEST_P(PolicyPrefsTest, CheckPolicyIndicators) {
  // Verifies that controlled setting indicators correctly show whether a pref's
  // value is recommended or enforced by a corresponding policy.
  const PolicyTestCase* policy_test_case =
      policy_test_cases_.Get(GetParam().name);
  ASSERT_TRUE(policy_test_case);
  const ScopedVector<PrefMapping>& pref_mappings =
      policy_test_case->pref_mappings();
  if (!policy_test_case->IsSupported() || pref_mappings.empty())
    return;
  bool has_indicator_tests = false;
  for (ScopedVector<PrefMapping>::const_iterator
           pref_mapping = pref_mappings.begin();
       pref_mapping != pref_mappings.end();
       ++pref_mapping) {
    if (!(*pref_mapping)->indicator_test_cases().empty()) {
      has_indicator_tests = true;
      break;
    }
  }
  if (!has_indicator_tests)
    return;
  LOG(INFO) << "Testing policy: " << policy_test_case->name();

  for (ScopedVector<PrefMapping>::const_iterator
           pref_mapping = pref_mappings.begin();
       pref_mapping != pref_mappings.end();
       ++pref_mapping) {
    const ScopedVector<IndicatorTestCase>&
        indicator_test_cases = (*pref_mapping)->indicator_test_cases();
    if (indicator_test_cases.empty())
      continue;

    ui_test_utils::NavigateToURL(browser(), GURL(kMainSettingsPage));
    if (!(*pref_mapping)->indicator_test_setup_js().empty()) {
      ASSERT_TRUE(content::ExecuteScript(
          chrome::GetActiveWebContents(browser()),
          (*pref_mapping)->indicator_test_setup_js()));
    }

    std::string indicator_selector = (*pref_mapping)->indicator_selector();
    if (indicator_selector.empty())
      indicator_selector = "[pref=\"" + (*pref_mapping)->pref() + "\"]";
    for (ScopedVector<IndicatorTestCase>::const_iterator
             indicator_test_case = indicator_test_cases.begin();
         indicator_test_case != indicator_test_cases.end();
         ++indicator_test_case) {
      // Check that no controlled setting indicator is visible when no value is
      // set by policy.
      PolicyMap policies;
      UpdateProviderPolicy(policies);
      VerifyControlledSettingIndicators(browser(), indicator_selector,
                                        "", "", false);
      // Check that the appropriate controlled setting indicator is shown when a
      // value is enforced by policy.
      policies.LoadFrom(&(*indicator_test_case)->policy(),
                        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
      UpdateProviderPolicy(policies);
      VerifyControlledSettingIndicators(browser(), indicator_selector,
                                        (*indicator_test_case)->value(),
                                        "policy",
                                        (*indicator_test_case)->readonly());

      if (!policy_test_case->can_be_recommended())
        continue;

      PrefService* local_state = g_browser_process->local_state();
      PrefService* user_prefs = browser()->profile()->GetPrefs();
      PrefService* prefs = (*pref_mapping)->is_local_state() ?
          local_state : user_prefs;
      // The preference must have been registered.
      const PrefService::Preference* pref =
          prefs->FindPreference((*pref_mapping)->pref().c_str());
      ASSERT_TRUE(pref);

      // Check that the appropriate controlled setting indicator is shown when a
      // value is recommended by policy and the user has not overridden the
      // recommendation.
      policies.LoadFrom(&(*indicator_test_case)->policy(),
                        POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER);
      UpdateProviderPolicy(policies);
      VerifyControlledSettingIndicators(browser(), indicator_selector,
                                        (*indicator_test_case)->value(),
                                        "recommended",
                                        (*indicator_test_case)->readonly());
      // Check that the appropriate controlled setting indicator is shown when a
      // value is recommended by policy and the user has overriddent the
      // recommendation.
      prefs->Set((*pref_mapping)->pref().c_str(), *pref->GetValue());
      VerifyControlledSettingIndicators(browser(), indicator_selector,
                                        (*indicator_test_case)->value(),
                                        "hasRecommendation",
                                        (*indicator_test_case)->readonly());
      prefs->ClearPref((*pref_mapping)->pref().c_str());
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    PolicyPrefsTestInstance,
    PolicyPrefsTest,
    testing::ValuesIn(GetChromePolicyDefinitionList()->begin,
                      GetChromePolicyDefinitionList()->end));

}  // namespace policy
