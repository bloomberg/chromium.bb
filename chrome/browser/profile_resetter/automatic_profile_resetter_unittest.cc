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
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

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
  MOCK_METHOD1(ReportPromptResult,
      void(AutomaticProfileResetter::PromptResult));

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterUnderTest);
};

class MockProfileResetterDelegate : public AutomaticProfileResetterDelegate {
 public:
  MockProfileResetterDelegate()
      : emulated_is_default_search_provider_managed_(false) {}
  virtual ~MockProfileResetterDelegate() {}

  MOCK_METHOD0(EnumerateLoadedModulesIfNeeded, void());
  MOCK_CONST_METHOD1(RequestCallbackWhenLoadedModulesAreEnumerated,
                     void(const base::Closure&));

  MOCK_METHOD0(LoadTemplateURLServiceIfNeeded, void());
  MOCK_CONST_METHOD1(RequestCallbackWhenTemplateURLServiceIsLoaded,
                     void(const base::Closure&));

  MOCK_METHOD0(FetchBrandcodedDefaultSettingsIfNeeded, void());
  MOCK_CONST_METHOD1(RequestCallbackWhenBrandcodedDefaultsAreFetched,
                     void(const base::Closure&));

  MOCK_CONST_METHOD0(OnGetLoadedModuleNameDigestsCalled, void());
  MOCK_CONST_METHOD0(OnGetDefaultSearchProviderDetailsCalled, void());
  MOCK_CONST_METHOD0(OnIsDefaultSearchProviderManagedCalled, void());
  MOCK_CONST_METHOD0(OnGetPrepopulatedSearchProvidersDetailsCalled, void());

  MOCK_METHOD0(TriggerPrompt, bool());
  MOCK_METHOD2(TriggerProfileSettingsReset, void(bool, const base::Closure&));
  MOCK_METHOD0(DismissPrompt, void());

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
    return emulated_is_default_search_provider_managed_;
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
        .WillOnce(testing::Invoke(ClosureInvoker));
    EXPECT_CALL(*this, RequestCallbackWhenTemplateURLServiceIsLoaded(_))
        .WillOnce(testing::Invoke(ClosureInvoker));
  }

  void ExpectCallsToGetterMethods() {
    EXPECT_CALL(*this, OnGetLoadedModuleNameDigestsCalled());
    EXPECT_CALL(*this, OnGetDefaultSearchProviderDetailsCalled());
    EXPECT_CALL(*this, OnIsDefaultSearchProviderManagedCalled());
    EXPECT_CALL(*this, OnGetPrepopulatedSearchProvidersDetailsCalled());
  }

  void ExpectCallToShowPrompt() {
    EXPECT_CALL(*this, TriggerPrompt()).WillOnce(testing::Return(true));
    EXPECT_CALL(*this, FetchBrandcodedDefaultSettingsIfNeeded());
  }

  void ExpectCallToTriggerReset(bool send_feedback) {
    EXPECT_CALL(*this, TriggerProfileSettingsReset(send_feedback, _))
        .WillOnce(testing::SaveArg<1>(&reset_completion_));
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

  void set_emulated_is_default_search_provider_managed(bool value) {
    emulated_is_default_search_provider_managed_ = value;
  }

  void EmulateProfileResetCompleted() {
    reset_completion_.Run();
  }

 private:
  base::DictionaryValue emulated_default_search_provider_details_;
  base::ListValue emulated_search_providers_details_;
  base::ListValue emulated_loaded_module_digests_;
  bool emulated_is_default_search_provider_managed_;
  base::Closure reset_completion_;

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
// prescribed by |emulate_satisfied_criterion_{1|2}|, and the reset is triggered
// when either of them is true. The bits in the combined status mask will be set
// according to whether or not the memento values received in the input were as
// expected.
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
//   "should_prompt":
//      (emulate_satisfied_criterion_1 || emulate_satisfied_criterion_2),
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
  bytecode += OP_STORE_BOOL(GetHash("should_prompt"),
                            EncodeBool(emulate_satisfied_criterion_1 ||
                                       emulate_satisfied_criterion_2));
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

// Constructs another evaluation program to specifically test that bits of the
// "satisfied_criteria_mask" are correctly assigned, and so is "should_prompt";
// and that reset is triggered iff the latter is true, regardless of the bits
// in the mask (so as to allow for a non-disjunctive compound criterion).
//
// More specifically, the output of the program will be as follows:
// {
//   "satisfied_criteria_mask_bitN": emulate_satisfied_odd_criteria,
//   "satisfied_criteria_mask_bitM": emulate_satisfied_even_criteria,
//   "combined_status_mask_bit1": emulate_should_prompt,
//   "should_prompt": emulate_should_prompt,
//   "memento_value_in_prefs": kTestMementoValue,
//   "memento_value_in_local_state": kTestMementoValue,
//   "memento_value_in_file": kTestMementoValue
// }
// ... such that N is {1,3,5} and M is {2,4}.
std::string ConstructProgramToExerciseCriteria(
    bool emulate_should_prompt,
    bool emulate_satisfied_odd_criteria,
    bool emulate_satisfied_even_criteria) {
  std::string bytecode;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit1"),
                            EncodeBool(emulate_satisfied_odd_criteria));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit3"),
                            EncodeBool(emulate_satisfied_odd_criteria));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit5"),
                            EncodeBool(emulate_satisfied_odd_criteria));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit2"),
                            EncodeBool(emulate_satisfied_even_criteria));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("satisfied_criteria_mask_bit4"),
                            EncodeBool(emulate_satisfied_even_criteria));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("should_prompt"),
                            EncodeBool(emulate_should_prompt));
  bytecode += OP_END_OF_SENTENCE;
  bytecode += OP_STORE_BOOL(GetHash("combined_status_mask_bit1"),
                            EncodeBool(emulate_should_prompt));
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
        field_trials_(new base::FieldTrialList(NULL)),
        memento_in_prefs_(new PreferenceHostedPromptMemento(profile())),
        memento_in_local_state_(new LocalStateHostedPromptMemento(profile())),
        memento_in_file_(new FileHostedPromptMementoSynchronous(profile())),
        experiment_group_name_(experiment_group_name),
        inject_data_through_variation_params_(false),
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
        kTestPreferencePath, std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }

  virtual void SetUp() OVERRIDE {
    chrome_variations::testing::ClearAllVariationParams();
    base::FieldTrialList::CreateFieldTrial(kAutomaticProfileResetStudyName,
                                           experiment_group_name_);
    resetter_.reset(
        new testing::StrictMock<AutomaticProfileResetterUnderTest>(profile()));
    mock_delegate_owned_.reset(
        new testing::StrictMock<MockProfileResetterDelegate>());
    mock_delegate_ = mock_delegate_owned_.get();

    ExpectAllMementoValuesEqualTo(std::string());
  }

  void SetTestingHashSeed(const std::string& hash_seed) {
    testing_hash_seed_ = hash_seed;
  }

  void SetTestingProgram(const std::string& source_code) {
    testing_program_ = source_code;
  }

  void AllowInjectingTestDataThroughVariationParams(bool value) {
    inject_data_through_variation_params_ = value;
  }

  void ExpectAllMementoValuesEqualTo(const std::string& value) {
    EXPECT_EQ(value, memento_in_prefs_->ReadValue());
    EXPECT_EQ(value, memento_in_local_state_->ReadValue());
    EXPECT_EQ(value, memento_in_file_->ReadValue());
  }

  void UnleashResetterAndWait() {
    if (inject_data_through_variation_params_) {
      std::map<std::string, std::string> variation_params;
      variation_params["program"] = testing_program_;
      variation_params["hash_seed"] = testing_hash_seed_;
      ASSERT_TRUE(chrome_variations::AssociateVariationParams(
          kAutomaticProfileResetStudyName,
          experiment_group_name_,
          variation_params));
    }
    resetter_->Initialize();
    resetter_->SetDelegateForTesting(
        mock_delegate_owned_.PassAs<AutomaticProfileResetterDelegate>());
    resetter_->SetTaskRunnerForWaitingForTesting(waiting_task_runner_);
    if (!inject_data_through_variation_params_) {
      resetter_->SetProgramForTesting(testing_program_);
      resetter_->SetHashSeedForTesting(testing_hash_seed_);
    }
    resetter_->Activate();

    if (waiting_task_runner_->HasPendingTask()) {
      ASSERT_EQ(base::TimeDelta::FromSeconds(55),
                waiting_task_runner_->NextPendingTaskDelay());
      waiting_task_runner_->RunPendingTasks();
    }
    base::RunLoop().RunUntilIdle();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  // Goes through an evaluation flow such that the reset criteria are satisfied.
  // Used to reduce boilerplate for tests that need to verify behavior during
  // the reset prompt flow.
  void OrchestrateThroughEvaluationFlow() {
    SetTestingProgram(ConstructProgram(true, true));
    SetTestingHashSeed(kTestHashSeed);

    mock_delegate().ExpectCallsToDependenciesSetUpMethods();
    mock_delegate().ExpectCallsToGetterMethods();
    mock_delegate().ExpectCallToShowPrompt();
    EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));

    UnleashResetterAndWait();

    EXPECT_TRUE(resetter().ShouldShowResetBanner());
    testing::Mock::VerifyAndClearExpectations(&resetter());
    testing::Mock::VerifyAndClearExpectations(&mock_delegate());
  }

  // Explicitly shut down the service to double-check that nothing explodes, but
  // first, verify expectations to make sure the service makes no more calls to
  // any mocked functions during or after shutdown.
  void VerifyExpectationsThenShutdownResetter() {
    testing::Mock::VerifyAndClearExpectations(&resetter());
    testing::Mock::VerifyAndClearExpectations(&mock_delegate());

    resetter_->Shutdown();
    resetter_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }
  TestingPrefServiceSimple* local_state() { return local_state_.Get(); }

  PreferenceHostedPromptMemento& memento_in_prefs() {
    return *memento_in_prefs_;
  }

  LocalStateHostedPromptMemento& memento_in_local_state() {
    return *memento_in_local_state_;
  }

  FileHostedPromptMementoSynchronous& memento_in_file() {
    return *memento_in_file_;
  }

  MockProfileResetterDelegate& mock_delegate() { return *mock_delegate_; }
  AutomaticProfileResetterUnderTest& resetter() { return *resetter_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::TestSimpleTaskRunner> waiting_task_runner_;
  ScopedTestingLocalState local_state_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<base::FieldTrialList> field_trials_;
  scoped_ptr<PreferenceHostedPromptMemento> memento_in_prefs_;
  scoped_ptr<LocalStateHostedPromptMemento> memento_in_local_state_;
  scoped_ptr<FileHostedPromptMementoSynchronous> memento_in_file_;

  std::string experiment_group_name_;
  std::string testing_program_;
  std::string testing_hash_seed_;
  bool inject_data_through_variation_params_;

  scoped_ptr<AutomaticProfileResetterUnderTest> resetter_;
  scoped_ptr<MockProfileResetterDelegate> mock_delegate_owned_;
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
  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  // No calls are expected to the delegate.

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
}

TEST_F(AutomaticProfileResetterTestDryRun, CriteriaNotSatisfied) {
  SetTestingProgram(ConstructProgramToExerciseCriteria(false, true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x1fu, 0x00u));

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
}

TEST_F(AutomaticProfileResetterTestDryRun, OddCriteriaSatisfied) {
  SetTestingProgram(ConstructProgramToExerciseCriteria(true, true, false));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x15u, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_NOT_TRIGGERED));

  UnleashResetterAndWait();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTestDryRun, EvenCriteriaSatisfied) {
  SetTestingProgram(ConstructProgramToExerciseCriteria(true, false, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x0au, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_NOT_TRIGGERED));

  UnleashResetterAndWait();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

#if defined(GOOGLE_CHROME_BUILD)
TEST_F(AutomaticProfileResetterTestDryRun, ProgramSetThroughVariationParams) {
  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);
  AllowInjectingTestDataThroughVariationParams(true);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_NOT_TRIGGERED));

  UnleashResetterAndWait();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}
#endif

TEST_F(AutomaticProfileResetterTestDryRun,
       ConditionsSatisfiedAndInvalidMementos) {
  memento_in_prefs().StoreValue(kTestInvalidMementoValue);
  memento_in_local_state().StoreValue(kTestInvalidMementoValue);
  memento_in_file().StoreValue(kTestInvalidMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_NOT_TRIGGERED));

  UnleashResetterAndWait();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTestDryRun, AlreadyHadPrefHostedMemento) {
  memento_in_prefs().StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x03u));

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs().ReadValue());
  EXPECT_EQ(std::string(), memento_in_local_state().ReadValue());
  EXPECT_EQ(std::string(), memento_in_file().ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, AlreadyHadLocalStateHostedMemento) {
  memento_in_local_state().StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x05u));

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  EXPECT_EQ(std::string(), memento_in_prefs().ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state().ReadValue());
  EXPECT_EQ(std::string(), memento_in_file().ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, AlreadyHadFileHostedMemento) {
  memento_in_file().StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x09u));

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  EXPECT_EQ(std::string(), memento_in_prefs().ReadValue());
  EXPECT_EQ(std::string(), memento_in_local_state().ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file().ReadValue());
}

TEST_F(AutomaticProfileResetterTestDryRun, DoNothingWhenResourcesAreMissing) {
  SetTestingProgram(std::string());
  SetTestingHashSeed(std::string());

  // No calls are expected to the delegate.

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
}

TEST_F(AutomaticProfileResetterTest, CriteriaNotSatisfied) {
  SetTestingProgram(ConstructProgramToExerciseCriteria(false, true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x1fu, 0x00u));

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
}

TEST_F(AutomaticProfileResetterTest, OddCriteriaSatisfied) {
  SetTestingProgram(ConstructProgramToExerciseCriteria(true, true, false));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  mock_delegate().ExpectCallToShowPrompt();
  EXPECT_CALL(resetter(), ReportStatistics(0x15u, 0x01u));

  UnleashResetterAndWait();

  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, EvenCriteriaSatisfied) {
  SetTestingProgram(ConstructProgramToExerciseCriteria(true, false, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  mock_delegate().ExpectCallToShowPrompt();
  EXPECT_CALL(resetter(), ReportStatistics(0x0au, 0x01u));

  UnleashResetterAndWait();

  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

#if defined(GOOGLE_CHROME_BUILD)
TEST_F(AutomaticProfileResetterTest, ProgramSetThroughVariationParams) {
  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);
  AllowInjectingTestDataThroughVariationParams(true);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  mock_delegate().ExpectCallToShowPrompt();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));

  UnleashResetterAndWait();
  resetter().NotifyDidShowResetBubble();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}
#endif

TEST_F(AutomaticProfileResetterTest, ConditionsSatisfiedAndInvalidMementos) {
  memento_in_prefs().StoreValue(kTestInvalidMementoValue);
  memento_in_local_state().StoreValue(kTestInvalidMementoValue);
  memento_in_file().StoreValue(kTestInvalidMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  mock_delegate().ExpectCallToShowPrompt();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));

  UnleashResetterAndWait();
  resetter().NotifyDidShowResetBubble();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PrefHostedMementoPreventsPrompt) {
  memento_in_prefs().StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x03u));

  UnleashResetterAndWait();

  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  EXPECT_EQ(kTestMementoValue, memento_in_prefs().ReadValue());
  EXPECT_EQ(std::string(), memento_in_local_state().ReadValue());
  EXPECT_EQ(std::string(), memento_in_file().ReadValue());
}

TEST_F(AutomaticProfileResetterTest, LocalStateHostedMementoPreventsPrompt) {
  memento_in_local_state().StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x05u));

  UnleashResetterAndWait();

  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  EXPECT_EQ(std::string(), memento_in_prefs().ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_local_state().ReadValue());
  EXPECT_EQ(std::string(), memento_in_file().ReadValue());
}

TEST_F(AutomaticProfileResetterTest, FileHostedMementoPreventsPrompt) {
  memento_in_file().StoreValue(kTestMementoValue);

  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x09u));

  UnleashResetterAndWait();

  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  EXPECT_EQ(std::string(), memento_in_prefs().ReadValue());
  EXPECT_EQ(std::string(), memento_in_local_state().ReadValue());
  EXPECT_EQ(kTestMementoValue, memento_in_file().ReadValue());
}

TEST_F(AutomaticProfileResetterTest, DoNothingWhenResourcesAreMissing) {
  SetTestingProgram(std::string());
  SetTestingHashSeed(std::string());

  // No calls are expected to the delegate.

  UnleashResetterAndWait();

  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
}

TEST_F(AutomaticProfileResetterTest, PromptSuppressed) {
  OrchestrateThroughEvaluationFlow();

  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
}

TEST_F(AutomaticProfileResetterTest, PromptNotSupported) {
  SetTestingProgram(ConstructProgram(true, true));
  SetTestingHashSeed(kTestHashSeed);

  mock_delegate().ExpectCallsToDependenciesSetUpMethods();
  mock_delegate().ExpectCallsToGetterMethods();
  EXPECT_CALL(mock_delegate(), TriggerPrompt())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(resetter(), ReportStatistics(0x03u, 0x01u));
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_NOT_TRIGGERED));

  UnleashResetterAndWait();

  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptIgnored) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptActionReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  mock_delegate().ExpectCallToTriggerReset(false);
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_ACTION_RESET));
  resetter().TriggerProfileReset(false /*send_feedback*/);
  testing::Mock::VerifyAndClearExpectations(&resetter());
  testing::Mock::VerifyAndClearExpectations(&mock_delegate());

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  mock_delegate().EmulateProfileResetCompleted();
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptActionResetWithFeedback) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  mock_delegate().ExpectCallToTriggerReset(true);
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_ACTION_RESET));
  resetter().TriggerProfileReset(true /*send_feedback*/);
  testing::Mock::VerifyAndClearExpectations(&resetter());
  testing::Mock::VerifyAndClearExpectations(&mock_delegate());

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  mock_delegate().EmulateProfileResetCompleted();
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptActionNoReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_ACTION_NO_RESET));
  resetter().SkipProfileReset();
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptFollowedByWebUIReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  resetter().NotifyDidOpenWebUIResetDialog();
  testing::Mock::VerifyAndClearExpectations(&mock_delegate());

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_FOLLOWED_BY_WEBUI_RESET));
  resetter().NotifyDidCloseWebUIResetDialog(true);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptFollowedByWebUINoReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  resetter().NotifyDidOpenWebUIResetDialog();
  testing::Mock::VerifyAndClearExpectations(&mock_delegate());

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_FOLLOWED_BY_WEBUI_NO_RESET));
  resetter().NotifyDidCloseWebUIResetDialog(false);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptFollowedByIncidentalWebUIReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  // Missing NotifyDidOpenWebUIResetDialog().
  // This can arise if a settings page was already opened at the time the prompt
  // was triggered, and this already opened dialog was used to initiate a reset
  // after having dismissed the prompt.

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_FOLLOWED_BY_WEBUI_RESET));
  resetter().NotifyDidCloseWebUIResetDialog(true);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptSuppressedButHadWebUIReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  resetter().NotifyDidOpenWebUIResetDialog();
  testing::Mock::VerifyAndClearExpectations(&mock_delegate());

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_NOT_SHOWN_BUBBLE_BUT_HAD_WEBUI_RESET));
  resetter().NotifyDidCloseWebUIResetDialog(true);
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, PromptSuppressedButHadWebUINoReset) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(mock_delegate(), DismissPrompt());
  resetter().NotifyDidOpenWebUIResetDialog();
  testing::Mock::VerifyAndClearExpectations(&mock_delegate());

  EXPECT_CALL(resetter(), ReportPromptResult(AutomaticProfileResetter::
      PROMPT_NOT_SHOWN_BUBBLE_BUT_HAD_WEBUI_NO_RESET));
  resetter().NotifyDidCloseWebUIResetDialog(false);
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  EXPECT_TRUE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, BannerDismissed) {
  OrchestrateThroughEvaluationFlow();

  EXPECT_CALL(resetter(), ReportPromptResult(
      AutomaticProfileResetter::PROMPT_SHOWN_BUBBLE));
  resetter().NotifyDidShowResetBubble();
  ExpectAllMementoValuesEqualTo(kTestMementoValue);
  testing::Mock::VerifyAndClearExpectations(&resetter());

  resetter().NotifyDidCloseWebUIResetBanner();

  EXPECT_TRUE(resetter().IsResetPromptFlowActive());
  EXPECT_FALSE(resetter().ShouldShowResetBanner());

  // Note: we use strict mocks, so this also checks the bubble is not closed.
  VerifyExpectationsThenShutdownResetter();
}

TEST_F(AutomaticProfileResetterTest, BannerDismissedWhilePromptSuppressed) {
  OrchestrateThroughEvaluationFlow();

  resetter().NotifyDidCloseWebUIResetBanner();

  EXPECT_TRUE(resetter().IsResetPromptFlowActive());
  EXPECT_FALSE(resetter().ShouldShowResetBanner());
  VerifyExpectationsThenShutdownResetter();

  ExpectAllMementoValuesEqualTo(std::string());
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
  uint32 expected_mask = HAS_EXPECTED_USER_PREFERENCE |
                         USER_PREFERENCE_IS_USER_CONTROLLED;
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
  uint32 expected_mask = HAS_EXPECTED_LOCAL_STATE_PREFERENCE |
                         LOCAL_STATE_IS_USER_CONTROLLED;
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
  mock_delegate().set_emulated_is_default_search_provider_managed(true);

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
