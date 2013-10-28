// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_mementos.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "chrome/browser/profile_resetter/jtl_instructions.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace {

const char kAutomaticProfileResetStudyName[] = "AutomaticProfileReset";
const char kStudyDisabledGroupName[] = "Disabled";
const char kStudyDryRunGroupName[] = "DryRun";
const char kStudyEnabledGroupName[] = "Enabled";

const char kTestHashSeed[] = "testing-hash-seed";
const char kTestMementoValue[] = "01234567890123456789012345678901";
const char kTestInvalidMementoValue[] = "12345678901234567890123456789012";

const char kTestPreferencePath[] = "testing.preference";
const char kTestPreferenceValue[] = "testing-preference-value";

const char kSearchURLAttributeKey[] = "search_url";
const char kTestSearchURL[] = "http://example.com/search?q={searchTerms}";
const char kTestSearchURL2[] = "http://google.com/?q={searchTerms}";

const char kTestModuleDigest[] = "01234567890123456789012345678901";
const char kTestModuleDigest2[] = "12345678901234567890123456789012";

// Helpers ------------------------------------------------------------------

// A testing version of the AutomaticProfileResetter that differs from the real
// one only in that it has its statistics reporting mocked out for verification.
class AutomaticProfileResetterUnderTest : public AutomaticProfileResetter {
 public:
  explicit AutomaticProfileResetterUnderTest(Profile* profile)
      : AutomaticProfileResetter(profile) {}
  virtual ~AutomaticProfileResetterUnderTest() {}

  MOCK_METHOD2(ReportStatistics, void(uint32, uint32));

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterUnderTest);
};

class MockProfileResetterDelegate : public AutomaticProfileResetterDelegate {
 public:
  MockProfileResetterDelegate()
      : emulated_default_search_provider_is_managed_(false) {}
  virtual ~MockProfileResetterDelegate() {}

  MOCK_METHOD0(EnumerateLoadedModulesIfNeeded, void());
  MOCK_CONST_METHOD1(RequestCallbackWhenLoadedModulesAreEnumerated,
                     void(const base::Closure&));

  MOCK_METHOD0(LoadTemplateURLServiceIfNeeded, void());
  MOCK_CONST_METHOD1(RequestCallbackWhenTemplateURLServiceIsLoaded,
                     void(const base::Closure&));

  MOCK_CONST_METHOD0(OnGetLoadedModuleNameDigestsCalled, void());
  MOCK_CONST_METHOD0(OnGetDefaultSearchProviderDetailsCalled, void());
  MOCK_CONST_METHOD0(OnIsDefaultSearchProviderManagedCalled, void());
  MOCK_CONST_METHOD0(OnGetPrepopulatedSearchProvidersDetailsCalled, void());

  MOCK_METHOD0(ShowPrompt, void());

  virtual scoped_ptr<base::ListValue>
      GetLoadedModuleNameDigests() const OVERRIDE {
    OnGetLoadedModuleNameDigestsCalled();
    return scoped_ptr<base::ListValue>(
        emulated_loaded_module_digests_.DeepCopy());
  }

  virtual scoped_ptr<base::DictionaryValue>
      GetDefaultSearchProviderDetails() const OVERRIDE {
    OnGetDefaultSearchProviderDetailsCalled();
    return scoped_ptr<base::DictionaryValue>(
        emulated_default_search_provider_details_.DeepCopy());
  }

  virtual bool IsDefaultSearchProviderManaged() const OVERRIDE {
    OnIsDefaultSearchProviderManagedCalled();
    return emulated_default_search_provider_is_managed_;
  }

  virtual scoped_ptr<base::ListValue>
      GetPrepopulatedSearchProvidersDetails() const OVERRIDE {
    OnGetPrepopulatedSearchProvidersDetailsCalled();
    return scoped_ptr<base::ListValue>(
        emulated_search_providers_details_.DeepCopy());
  }

  static void ClosureInvoker(const base::Closure& closure) { closure.Run(); }

  void ExpectCallsToDependenciesSetUpMethods() {
    EXPECT_CALL(*this, EnumerateLoadedModulesIfNeeded());
    EXPECT_CALL(*this, LoadTemplateURLServiceIfNeeded());
    EXPECT_CALL(*this, RequestCallbackWhenLoadedModulesAreEnumerated(_))
        .WillOnce(Invoke(ClosureInvoker));
    EXPECT_CALL(*this, RequestCallbackWhenTemplateURLServiceIsLoaded(_))
        .WillOnce(Invoke(ClosureInvoker));
  }

  void ExpectCallsToGetterMethods() {
    EXPECT_CALL(*this, OnGetLoadedModuleNameDigestsCalled());
    EXPECT_CALL(*this, OnGetDefaultSearchProviderDetailsCalled());
    EXPECT_CALL(*this, OnIsDefaultSearchProviderManagedCalled());
    EXPECT_CALL(*this, OnGetPrepopulatedSearchProvidersDetailsCalled());
  }

  base::DictionaryValue& emulated_default_search_provider_details() {
    return emulated_default_search_provider_details_;
  }

  base::ListValue& emulated_search_providers_details() {
    return emulated_search_providers_details_;
  }

  base::ListValue& emulated_loaded_module_digests() {
    return emulated_loaded_module_digests_;
  }

  void set_emulated_default_search_provider_is_managed(bool value) {
    emulated_default_search_provider_is_managed_ = value;
  }

 private:
  base::DictionaryValue emulated_default_search_provider_details_;
  base::ListValue emulated_search_providers_details_;
  base::ListValue emulated_loaded_module_digests_;
  bool emulated_default_search_provider_is_managed_;

  DISALLOW_COPY_AND_ASSIGN(MockProfileResetterDelegate);
};

class FileHostedPromptMementoSynchronous : protected FileHostedPromptMemento {
 public:
  explicit FileHostedPromptMementoSynchronous(Profile* profile)
      : FileHostedPromptMemento(profile) {}

  std::string ReadValue() const {
    std::string result;
    FileHostedPromptMemento::ReadValue(base::Bind(&AssignArgumentTo, &result));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  void StoreValue(const std::string& value) {
    FileHostedPromptMemento::StoreValue(value);
    base::RunLoop().RunUntilIdle();
  }

 private:
  static void AssignArgumentTo(std::string* target, const std::string& value) {
    *target = value;
  }

  DISALLOW_COPY_AND_ASSIGN(FileHostedPromptMementoSynchronous);
};

std::string GetHash(const std::string& input) {
  return jtl_foundation::Hasher(kTestHashSeed).GetHash(input);
}

// Encodes a Boolean argument value into JTL bytecode.
std::string EncodeBool(bool value) { return value ? VALUE_TRUE : VALUE_FALSE; }

// Constructs a simple evaluation program to test that basic input/output works
// well. It will emulate a scenario in which the reset criteria are satisfied as
// prescribed by |emulate_satisfied_criterion_{1|2}|, and will set bits in the
// combined status mask according to whether or not the memento values received
// in the input were as expected.
//
// More specifically, the output of the program will be as follows:
// {
//   "satisfied_criteria_mask_bit1": emulate_satisfied_criterion_1,
//   "satisfied_criteria_mask_bit2": emulate_satisfied_criterion_2,
//   "combined_status_mask_bit1":
//      (emulate_satisfied_criterion_1 || emulate_satisfied_criterion_2),
//   "combined_status_mask_bit2":
//      (input["memento_value_in_prefs"] == kTestMementoValue),
//   "combined_status_mask_bit3":
//      (input["memento_value_in_local_state"] == kTestMementoValue),
//   "combined_status_mask_bit4":
//      (input["memento_value_in_file"] == kTestMementoValue),
//   "had_prompted_already": <OR-combination of above three>,
//   "memento_value_in_prefs": kTestMementoValue,
//   "memento_value_in_local_state": kTestMementoValue,
//   "memento_value_in_file": kTestMementoValue
// }
std::string ConstructProgram(bool emulate_satisfied_criterion_1,
                             bool emulate_satisfied_criterion_2) {
  std::string bytecode;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit1"),
                            EncodeBool(emulate_satisfied_criterion_1));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit2"),
                            EncodeBool(emulate_satisfied_criterion_2));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit1"),
                            EncodeBool(emulate_satisfied_criterion_1 ||
                                       emulate_satisfied_criterion_2));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("memento_value_in_prefs"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestMementoValue));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit2"), VALUE_TRUE);
  bytecode += OP_STORE_BOOL(GetHash("had_prompted_already"), VALUE_TRUE);
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("memento_value_in_local_state"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestMementoValue));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit3"), VALUE_TRUE);
  bytecode += OP_STORE_BOOL(GetHash("had_prompted_already"), VALUE_TRUE);
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("memento_value_in_file"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestMementoValue));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit4"), VALUE_TRUE);
  bytecode += OP_STORE_BOOL(GetHash("had_prompted_already"), VALUE_TRUE);
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_HASH(GetHash("memento_value_in_prefs"),
                            kTestMementoValue);
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_HASH(GetHash("memento_value_in_local_state"),
                            kTestMementoValue);
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_HASH(GetHash("memento_value_in_file"),
                            kTestMementoValue);
  bytecode += OP_END_OF_SENTENCE;
  return bytecode;
}

// Constructs another evaluation program to specifically test that local state
// and user preference values are included in the input as expected. We will
// re-purpose the output bitmasks to channel out information about the outcome
// of the checks.
//
// More specifically, the output of the program will be as follows:
// {
//   "combined_status_mask_bit1":
//       (input["preferences.testing.preference"] == kTestPreferenceValue)
//   "combined_status_mask_bit2":
//       (input["local_state.testing.preference"] == kTestPreferenceValue)
//   "combined_status_mask_bit3": input["preferences_iuc.testing.preference"]
//   "combined_status_mask_bit4": input["local_state_iuc.testing.preference"]
// }
std::string ConstructProgramToCheckPreferences() {
  std::string bytecode;
  bytecode += OP_NAVIGATE(GetHash("preferences"));
  bytecode += OP_NAVIGATE(GetHash("testing"));
  bytecode += OP_NAVIGATE(GetHash("preference"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestPreferenceValue));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit1"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("local_state"));
  bytecode += OP_NAVIGATE(GetHash("testing"));
  bytecode += OP_NAVIGATE(GetHash("preference"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestPreferenceValue));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit2"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("preferences_iuc"));
  bytecode += OP_NAVIGATE(GetHash("testing"));
  bytecode += OP_NAVIGATE(GetHash("preference"));
  bytecode += OP_COMPARE_NODE_BOOL(EncodeBool(true));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit3"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("local_state_iuc"));
  bytecode += OP_NAVIGATE(GetHash("testing"));
  bytecode += OP_NAVIGATE(GetHash("preference"));
  bytecode += OP_COMPARE_NODE_BOOL(EncodeBool(true));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit4"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  return bytecode;
}

// Legend for the bitmask returned by the above program.
enum CombinedStatusMaskLegendForCheckingPreferences {
  HAS_EXPECTED_USER_PREFERENCE = 1 << 0,
  HAS_EXPECTED_LOCAL_STATE_PREFERENCE = 1 << 1,
  USER_PREFERENCE_IS_USER_CONTROLLED = 1 << 2,
  LOCAL_STATE_IS_USER_CONTROLLED = 1 << 3,
};

// Constructs yet another evaluation program to specifically test that default
// and pre-populated search engines are included in the input as expected. We
// will re-purpose the output bitmasks to channel out information about the
// outcome of the checks.
//
// More specifically, the output of the program will be as follows:
// {
//   "combined_status_mask_bit1":
//       (input["default_search_provider.search_url"] == kTestSearchURL)
//   "combined_status_mask_bit2": input["default_search_provider_iuc"]
//   "combined_status_mask_bit3":
//       (input["search_providers.*.search_url"] == kTestSearchURL)
//   "combined_status_mask_bit4":
//       (input["search_providers.*.search_url"] == kTestSearchURL2)
// }
std::string ConstructProgramToCheckSearchEngines() {
  std::string bytecode;
  bytecode += OP_NAVIGATE(GetHash("default_search_provider"));
  bytecode += OP_NAVIGATE(GetHash("search_url"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestSearchURL));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit1"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("default_search_provider_iuc"));
  bytecode += OP_COMPARE_NODE_BOOL(EncodeBool(true));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit2"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("search_providers"));
  bytecode += OP_NAVIGATE_ANY;
  bytecode += OP_NAVIGATE(GetHash("search_url"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestSearchURL));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit3"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("search_providers"));
  bytecode += OP_NAVIGATE_ANY;
  bytecode += OP_NAVIGATE(GetHash("search_url"));
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestSearchURL2));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit4"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  return bytecode;
}

// Legend for the bitmask returned by the above program.
enum CombinedStatusMaskLegendForCheckingSearchEngines {
  HAS_EXPECTED_DEFAULT_SEARCH_PROVIDER = 1 << 0,
  DEFAULT_SEARCH_PROVIDER_IS_USER_CONTROLLED = 1 << 1,
  HAS_EXPECTED_PREPOPULATED_SEARCH_PROVIDER_1 = 1 << 2,
  HAS_EXPECTED_PREPOPULATED_SEARCH_PROVIDER_2 = 1 << 3,
};

// Constructs yet another evaluation program to specifically test that loaded
// module digests are included in the input as expected. We will re-purpose the
// output bitmasks to channel out information about the outcome of the checks.
//
// More specifically, the output of the program will be as follows:
// {
//   "combined_status_mask_bit1":
//       (input["loaded_modules.*"] == kTestModuleDigest)
//   "combined_status_mask_bit2":
//       (input["loaded_modules.*"] == kTestModuleDigest2)
// }
std::string ConstructProgramToCheckLoadedModuleDigests() {
  std::string bytecode;
  bytecode += OP_NAVIGATE(GetHash("loaded_modules"));
  bytecode += OP_NAVIGATE_ANY;
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestModuleDigest));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit1"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_NAVIGATE(GetHash("loaded_modules"));
  bytecode += OP_NAVIGATE_ANY;
  bytecode += OP_COMPARE_NODE_HASH(GetHash(kTestModuleDigest2));
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit2"),
                            EncodeBool(true));
  bytecode += OP_END_OF_SENTENCE;
  return bytecode;
}

// Legend for the bitmask returned by the above program.
enum CombinedStatusMaskLegendForCheckingLoadedModules {
  HAS_EXPECTED_MODULE_DIGEST_1 = 1 << 0,
  HAS_EXPECTED_MODULE_DIGEST_2 = 1 << 1,
};

// Test fixtures -------------------------------------------------------------

class AutomaticProfileResetterTestBase : public testing::Test {
 protected:
  explicit AutomaticProfileResetterTestBase(
      const std::string& experiment_group_name)
      : waiting_task_runner_(new base::TestSimpleTaskRunner),
        local_state_(TestingBrowserProcess::GetGlobal()),
        profile_(new TestingProfile()),
        experiment_group_name_(experiment_group_name),
        mock_delegate_(NULL) {
    // Make sure the factory is not optimized away, so whatever preferences it
    // wants to register will actually get registered.
    AutomaticProfileResetterFactory::GetInstance();

    // Register some additional local state preferences for testing purposes.
    PrefRegistrySimple* local_state_registry = local_state_.Get()->registry();
    DCHECK(local_state_registry);
    local_state_registry->RegisterStringPref(kTestPreferencePath, "");

    // Register some additional user preferences for testing purposes.
    user_prefs::PrefRegistrySyncable* user_prefs_registry =
        profile_->GetTestingPrefService()->registry();
    DCHECK(user_prefs_registry);
    user_prefs_registry->RegisterStringPref(
        kTestPreferencePath,
        "",
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }

  virtual void SetUp() OVERRIDE {
    field_trials_.reset(new base::FieldTrialList(NULL));
    base::FieldTrialList::CreateFieldTrial(kAutomaticProfileResetStudyName,
                                           experiment_group_name_);

    resetter_.reset(new testing::StrictMock<AutomaticProfileResetterUnderTest>(
        profile_.get()));

    scoped_ptr<MockProfileResetterDelegate> mock_delegate(
        new testing::StrictMock<MockProfileResetterDelegate>());
    mock_delegate_ = mock_delegate.get();
    resetter_->SetDelegateForTesting(
        mock_delegate.PassAs<AutomaticProfileResetterDelegate>());
    resetter_->SetTaskRunnerForWaitingForTesting(waiting_task_runner_);
  }

  void SetTestingHashSeed(const std::string& hash_seed) {
    testing_hash_seed_ = hash_seed;
  }

  void SetTestingProgram(const std::string& source_code) {
    testing_program_ = source_code;
  }

  void UnleashResetterAndWait() {
    resetter_->SetHashSeedForTesting(testing_hash_seed_);
    resetter_->SetProgramForTesting(testing_program_);

    resetter_->Activate();

    if (waiting_task_runner_->HasPendingTask()) {
      EXPECT_EQ(base::TimeDelta::FromSeconds(55),
                waiting_task_runner_->NextPendingTaskDelay());
      waiting_task_runner_->RunPendingTasks();
    }
    base::RunLoop().RunUntilIdle();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  TestingProfile* profile() { return profile_.get(); }
  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }

  MockProfileResetterDelegate& mock_delegate() { return *mock_delegate_; }
  AutomaticProfileResetterUnderTest& resetter() { return *resetter_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::TestSimpleTaskRunner> waiting_task_runner_;
  ScopedTestingLocalState local_state_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<base::FieldTrialList> field_trials_;
  std::string experiment_group_name_;
  std::string testing_program_;
  std::string testing_hash_seed_;

  scoped_ptr<AutomaticProfileResetterUnderTest> resetter_;
  MockProfileResetterDelegate* mock_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterTestBase);
};

class AutomaticProfileResetterTest : public AutomaticProfileResetterTestBase {
 protected:
  AutomaticProfileResetterTest()
      : AutomaticProfileResetterTestBase(kStudyEnabledGroupName) {}
};

class AutomaticProfileResetterTestDryRun
    : public AutomaticProfileResetterTestBase {
 protected:
  AutomaticProfileResetterTestDryRun()
      : AutomaticProfileResetterTestBase(kStudyDryRunGroupName) {}
};

class AutomaticProfileResetterTestDisabled
    : public AutomaticProfileResetterTestBase {
 protected:
  AutomaticProfileResetterTestDisabled()
      : AutomaticProfileResetterTestBase(kStudyDisabledGroupName) {}
};

// Tests ---------------------------------------------------------------------

TEST_F(AutomaticProfileResetterTestDisabled, NothingIsDoneWhenDisabled) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  // No calls are expected to the delegate.

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, ConditionsNotSatisfied) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(false, false));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, 0x00u));

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, OneConditionSatisfied) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(true, false));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x01u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, OtherConditionSatisfied) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(false, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x02u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun,
       ConditionsSatisfiedAndInvalidMementos) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_prefs.StoreValue(kTestInvalidMementoValue);
  memento_in_local_state.StoreValue(kTestInvalidMementoValue);
  memento_in_file.StoreValue(kTestInvalidMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, AlreadyHadPrefHostedMemento) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_prefs.StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x03u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, AlreadyHadLocalStateHostedMemento) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_local_state.StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x05u));

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, AlreadyHadFileHostedMemento) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_file.StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x09u));

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, DoNothingWhenResourcesAreMissing) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  SetTestingProgram("");
  SetTestingHashSeed("");

  // No calls are expected to the delegate.

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, ConditionsNotSatisfied) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(false, false));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, 0x00u));

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, OneConditionSatisfied) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(true, false));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(mock_delegate(), ShowPrompt());
  EXPECT_CALL(resetter(), ReportStatistics(0x01u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, OtherConditionSatisfied) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());

  SetTestingProgram(ConstructProgram(false, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(mock_delegate(), ShowPrompt());
  EXPECT_CALL(resetter(), ReportStatistics(0x02u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, ConditionsSatisfiedAndInvalidMementos) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_prefs.StoreValue(kTestInvalidMementoValue);
  memento_in_local_state.StoreValue(kTestInvalidMementoValue);
  memento_in_file.StoreValue(kTestInvalidMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(mock_delegate(), ShowPrompt());
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, PrefHostedMementoPreventsPrompt) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_prefs.StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x03u));

  UnleashResetterAndWait();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, LocalStateHostedMementoPreventsPrompt) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_local_state.StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x05u));

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, FileHostedMementoPreventsPrompt) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  memento_in_file.StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x09u));

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file.ReadValue());
}

TEST_F(AutomaticProfileResetterTest, DoNothingWhenResourcesAreMissing) {
  PreferenceHostedPromptMemento memento_in_prefs(profile());
  LocalStateHostedPromptMemento memento_in_local_state(profile());
  FileHostedPromptMementoSynchronous memento_in_file(profile());

  SetTestingProgram("");
  SetTestingHashSeed("");

  // No calls are expected to the delegate.

  UnleashResetterAndWait();

  EXPECT_EQ("", memento_in_prefs.ReadValue());
  EXPECT_EQ("", memento_in_local_state.ReadValue());
  EXPECT_EQ("", memento_in_file.ReadValue());
}

// Please see comments above ConstructProgramToCheckPreferences() to understand
// how the following tests work.

TEST_F(AutomaticProfileResetterTest, InputUserPreferencesCorrect) {
  SetTestingProgram(ConstructProgramToCheckPreferences());
  SetTestingHashSeed(kTestHashSeed);

  PrefService* prefs = profile()->GetPrefs();
  prefs->SetString(kTestPreferencePath, kTestPreferenceValue);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask =
      HAS_EXPECTED_USER_PREFERENCE | USER_PREFERENCE_IS_USER_CONTROLLED;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

TEST_F(AutomaticProfileResetterTest, InputLocalStateCorrect) {
  SetTestingProgram(ConstructProgramToCheckPreferences());
  SetTestingHashSeed(kTestHashSeed);

  PrefService* prefs = local_state();
  prefs->SetString(kTestPreferencePath, kTestPreferenceValue);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask =
      HAS_EXPECTED_LOCAL_STATE_PREFERENCE | LOCAL_STATE_IS_USER_CONTROLLED;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

TEST_F(AutomaticProfileResetterTest, InputManagedUserPreferencesCorrect) {
  SetTestingProgram(ConstructProgramToCheckPreferences());
  SetTestingHashSeed(kTestHashSeed);

  TestingPrefServiceSyncable* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(kTestPreferencePath,
                        new base::StringValue(kTestPreferenceValue));

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask = HAS_EXPECTED_USER_PREFERENCE;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

TEST_F(AutomaticProfileResetterTest, InputManagedLocalStateCorrect) {
  SetTestingProgram(ConstructProgramToCheckPreferences());
  SetTestingHashSeed(kTestHashSeed);

  TestingPrefServiceSimple* prefs = local_state();
  prefs->SetManagedPref(kTestPreferencePath,
                        new base::StringValue(kTestPreferenceValue));

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask = HAS_EXPECTED_LOCAL_STATE_PREFERENCE;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

// Please see comments above ConstructProgramToCheckSearchEngines() to
// understand how the following tests work.

TEST_F(AutomaticProfileResetterTest, InputDefaultSearchProviderCorrect) {
  SetTestingProgram(ConstructProgramToCheckSearchEngines());
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().emulated_default_search_provider_details().SetString(
      kSearchURLAttributeKey, kTestSearchURL);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask = HAS_EXPECTED_DEFAULT_SEARCH_PROVIDER |
                         DEFAULT_SEARCH_PROVIDER_IS_USER_CONTROLLED;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

TEST_F(AutomaticProfileResetterTest, InputSearchProviderManagedCorrect) {
  SetTestingProgram(ConstructProgramToCheckSearchEngines());
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().emulated_default_search_provider_details().SetString(
      kSearchURLAttributeKey, kTestSearchURL);
  mock_delegate().set_emulated_default_search_provider_is_managed(true);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask = HAS_EXPECTED_DEFAULT_SEARCH_PROVIDER;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

TEST_F(AutomaticProfileResetterTest, InputSearchProvidersCorrect) {
  SetTestingProgram(ConstructProgramToCheckSearchEngines());
  SetTestingHashSeed(kTestHashSeed);

  base::DictionaryValue* search_provider_1 = new base::DictionaryValue;
  base::DictionaryValue* search_provider_2 = new base::DictionaryValue;
  search_provider_1->SetString(kSearchURLAttributeKey, kTestSearchURL);
  search_provider_2->SetString(kSearchURLAttributeKey, kTestSearchURL2);
  mock_delegate().emulated_search_providers_details().Append(search_provider_1);
  mock_delegate().emulated_search_providers_details().Append(search_provider_2);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask = DEFAULT_SEARCH_PROVIDER_IS_USER_CONTROLLED |
                         HAS_EXPECTED_PREPOPULATED_SEARCH_PROVIDER_1 |
                         HAS_EXPECTED_PREPOPULATED_SEARCH_PROVIDER_2;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

// Please see comments above ConstructProgramToCheckLoadedModuleDigests() to
// understand how the following tests work.

TEST_F(AutomaticProfileResetterTest, InputModuleDigestsCorrect) {
  SetTestingProgram(ConstructProgramToCheckLoadedModuleDigests());
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().emulated_loaded_module_digests().AppendString(
      kTestModuleDigest);
  mock_delegate().emulated_loaded_module_digests().AppendString(
      kTestModuleDigest2);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  uint32 expected_mask =
      HAS_EXPECTED_MODULE_DIGEST_1 | HAS_EXPECTED_MODULE_DIGEST_2;
  EXPECT_CALL(resetter(), ReportStatistics(0x00u, expected_mask));

  UnleashResetterAndWait();
}

}  // namespace
