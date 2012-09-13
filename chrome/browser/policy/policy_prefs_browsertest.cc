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
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/base_paths.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#endif

using testing::Return;

namespace policy {

namespace {

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

}  // namespace

class PolicyPrefsTest : public InProcessBrowserTest {
 protected:
  PolicyPrefsTest() {}
  virtual ~PolicyPrefsTest() {}

  // Loads policy_test_cases.json and builds a map of test cases.
  static void SetUpTestCase() {
#if defined(OS_MACOSX)
    // Ugly hack to work around http://crbug.com/63183, since this uses the
    // PathService from GetTestFilePath() above before BrowserTestBase() and
    // InProcessBrowserTest() are invoked. Those ctors include similar hacks.
    base::mac::SetOverrideAmIBundled(true);
    FilePath chrome_path;
    CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
    FilePath fixed_chrome_path =
        chrome_path.DirName().Append(chrome::kBrowserProcessExecutablePath);
    CHECK(PathService::Override(base::FILE_EXE, fixed_chrome_path));
#endif

    FilePath path = ui_test_utils::GetTestFilePath(
        FilePath(FILE_PATH_LITERAL("policy")),
        FilePath(FILE_PATH_LITERAL("policy_test_cases.json")));
    std::string json;
    ASSERT_TRUE(file_util::ReadFileToString(path, &json));
    int error_code = -1;
    std::string error_string;
    scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
        json, base::JSON_ALLOW_TRAILING_COMMAS, &error_code, &error_string));
    ASSERT_TRUE(value.get())
        << "Error parsing policy_test_cases.json: " << error_string;
    base::DictionaryValue* dict = NULL;
    ASSERT_TRUE(value->GetAsDictionary(&dict));
    policy_test_cases_ = new std::map<std::string, PolicyTestCase*>();
    const PolicyDefinitionList* list = GetChromePolicyDefinitionList();
    for (const PolicyDefinitionList::Entry* policy = list->begin;
         policy != list->end; ++policy) {
      PolicyTestCase* test_case = GetTestCase(dict, policy->name);
      if (test_case)
        (*policy_test_cases_)[policy->name] = test_case;
    }

#if defined(OS_MACOSX)
    // Restore |chrome_path| so that the fix in InProcessBrowserTest() works.
    CHECK(PathService::Override(base::FILE_EXE, chrome_path));
#endif
  }

  static void TearDownTestCase() {
    STLDeleteValues(policy_test_cases_);
    delete policy_test_cases_;
  }

  static PolicyTestCase* GetTestCase(const base::DictionaryValue* tests,
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

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  static std::map<std::string, PolicyTestCase*>* policy_test_cases_;
  MockConfigurationPolicyProvider provider_;
};

std::map<std::string, PolicyTestCase*>* PolicyPrefsTest::policy_test_cases_ = 0;

IN_PROC_BROWSER_TEST_F(PolicyPrefsTest, AllPoliciesHaveATestCase) {
  // Verifies that all known policies have a test case in the JSON file.
  // This test fails when a policy is added to
  // chrome/app/policy/policy_templates.json but a test case is not added to
  // chrome/test/data/policy/policy_test_cases.json.
  const PolicyDefinitionList* list = GetChromePolicyDefinitionList();
  for (const PolicyDefinitionList::Entry* policy = list->begin;
       policy != list->end; ++policy) {
    EXPECT_TRUE(ContainsKey(*policy_test_cases_, policy->name))
        << "Missing policy test case for: " << policy->name;
  }
}

IN_PROC_BROWSER_TEST_F(PolicyPrefsTest, PolicyToPrefsMapping) {
  // Verifies that policies make their corresponding preferences become managed,
  // and that the user can't override that setting.
  const PolicyMap kNoPolicies;
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  PrefService* local_state = g_browser_process->local_state();
  std::map<std::string, PolicyTestCase*>::iterator it;
  for (it = policy_test_cases_->begin();
       it != policy_test_cases_->end(); ++it) {
    PolicyTestCase* test_case = it->second;
    if (!test_case->IsSupported() || test_case->pref().empty())
      continue;
    LOG(INFO) << "Testing policy: " << test_case->name();
    // Clear policies.
    provider_.UpdateChromePolicy(kNoPolicies);

    PrefService* prefs =
        test_case->is_local_state() ? local_state : profile_prefs;
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
}

}  // namespace policy
