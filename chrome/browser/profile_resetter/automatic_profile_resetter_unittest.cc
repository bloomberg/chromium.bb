// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_mementos.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "chrome/browser/profile_resetter/jtl_instructions.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAutomaticProfileResetStudyName[] = "AutomaticProfileReset";
const char kStudyDisabledGroupName[] = "Disabled";
const char kStudyDryRunGroupName[] = "DryRun";
const char kStudyEnabledGroupName[] = "Enabled";

const char kTestHashSeed[] = "testing-hash-seed";
const char kTestMementoValue[] = "01234567890123456789012345678901";
const char kTestInvalidMementoValue[] = "12345678901234567890123456789012";

// Helpers ------------------------------------------------------------------

class MockProfileResetterDelegate : public AutomaticProfileResetterDelegate {
 public:
  MockProfileResetterDelegate() {}
  virtual ~MockProfileResetterDelegate() {}

  MOCK_METHOD0(ShowPrompt, void());
  MOCK_METHOD2(ReportStatistics, void(uint32, uint32));

 private:
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

// Constructs a simple evaluation program to test that input/output works well.
// It will emulate a scenario in which the reset criteria are satisfied as
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

// Test fixtures -------------------------------------------------------------

class AutomaticProfileResetterTestBase : public testing::Test {
 protected:
  explicit AutomaticProfileResetterTestBase(
      const std::string& experiment_group_name)
      : local_state_(TestingBrowserProcess::GetGlobal()),
        experiment_group_name_(experiment_group_name),
        mock_delegate_(NULL) {
    // Make sure the factory is not optimized away, so prefs get registered.
    AutomaticProfileResetterFactory::GetInstance();
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    field_trials_.reset(new base::FieldTrialList(NULL));
    base::FieldTrialList::CreateFieldTrial(kAutomaticProfileResetStudyName,
                                           experiment_group_name_);
    mock_delegate_ = new testing::StrictMock<MockProfileResetterDelegate>();
    resetter_.reset(new AutomaticProfileResetter(profile_.get()));
  }

  void SetTestingHashSeed(const std::string& hash_seed) {
    testing_hash_seed_ = hash_seed;
  }

  void SetTestingProgram(const std::string& source_code) {
    testing_program_ = source_code;
  }

  void UnleashResetterAndWait() {
    resetter_->Initialize();
    resetter_->SetDelegateForTesting(mock_delegate_);  // Takes ownership.
    resetter_->SetHashSeedForTesting(testing_hash_seed_);
    resetter_->SetProgramForTesting(testing_program_);
    base::RunLoop().RunUntilIdle();
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  TestingProfile* profile() { return profile_.get(); }

  MockProfileResetterDelegate& mock_delegate() { return *mock_delegate_; }
  AutomaticProfileResetter* resetter() { return resetter_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ScopedTestingLocalState local_state_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<base::FieldTrialList> field_trials_;
  std::string experiment_group_name_;
  std::string testing_program_;
  std::string testing_hash_seed_;

  scoped_ptr<AutomaticProfileResetter> resetter_;
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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x00u, 0x00u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x01u, 0x01u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x02u, 0x01u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x01u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x03u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x05u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x09u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x00u, 0x00u));

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

  EXPECT_CALL(mock_delegate(), ShowPrompt());
  EXPECT_CALL(mock_delegate(), ReportStatistics(0x01u, 0x01u));

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

  EXPECT_CALL(mock_delegate(), ShowPrompt());
  EXPECT_CALL(mock_delegate(), ReportStatistics(0x02u, 0x01u));

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

  EXPECT_CALL(mock_delegate(), ShowPrompt());
  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x01u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x03u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x05u));

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

  EXPECT_CALL(mock_delegate(), ReportStatistics(0x03u, 0x09u));

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

}  // namespace
