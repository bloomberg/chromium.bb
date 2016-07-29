// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/ruleset_service.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/core/browser/ruleset_distributor.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

const char kTestContentVersion1[] = "1.2.3.4";
const char kTestContentVersion2[] = "1.2.3.5";

const char kTestDisallowedSuffix1[] = "foo";
const char kTestDisallowedSuffix2[] = "bar";

class MockRulesetDistributor : public RulesetDistributor {
 public:
  MockRulesetDistributor() = default;
  ~MockRulesetDistributor() override = default;

  void PublishNewVersion(base::File ruleset_data) override {
    published_rulesets_.push_back(std::move(ruleset_data));
  }

  std::vector<base::File>& published_rulesets() { return published_rulesets_; }

 private:
  std::vector<base::File> published_rulesets_;

  DISALLOW_COPY_AND_ASSIGN(MockRulesetDistributor);
};

std::vector<uint8_t> ReadFileContents(base::File* file) {
  size_t length = base::checked_cast<size_t>(file->GetLength());
  std::vector<uint8_t> contents(length);
  static_assert(sizeof(uint8_t) == sizeof(char), "Expected char = byte.");
  file->Read(0, reinterpret_cast<char*>(contents.data()),
             base::checked_cast<int>(length));
  return contents;
}

}  // namespace

using testing::TestRulesetPair;
using testing::TestRulesetCreator;

class SubresourceFilteringRulesetServiceTest : public ::testing::Test {
 public:
  SubresourceFilteringRulesetServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        mock_distributor_(nullptr) {}

 protected:
  void SetUp() override {
    IndexedRulesetVersion::RegisterPrefs(pref_service_.registry());

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    ResetService(CreateRulesetService());
    RunUntilIdle();

    ASSERT_NO_FATAL_FAILURE(
        ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            kTestDisallowedSuffix1, &test_ruleset_1_));
    ASSERT_NO_FATAL_FAILURE(
        ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            kTestDisallowedSuffix2, &test_ruleset_2_));
  }

  std::unique_ptr<RulesetService> CreateRulesetService() {
    return base::WrapUnique(
        new RulesetService(&pref_service_, task_runner_, base_dir()));
  }

  void ResetService(std::unique_ptr<RulesetService> new_service = nullptr) {
    service_ = std::move(new_service);
    if (service_) {
      service_->RegisterDistributor(
          base::WrapUnique(mock_distributor_ = new MockRulesetDistributor()));
    } else {
      mock_distributor_ = nullptr;
    }
  }

  void IndexAndStoreAndPublishUpdatedRuleset(
      const TestRulesetPair& test_ruleset_pair,
      const std::string& new_content_version) {
    service()->IndexAndStoreAndPublishRulesetVersionIfNeeded(
        test_ruleset_pair.unindexed.path, new_content_version);
  }

  void WritePreexistingRuleset(const TestRulesetPair& test_ruleset_pair,
                               const IndexedRulesetVersion& indexed_version) {
    RulesetService::WriteRuleset(base_dir(), indexed_version,
                                 test_ruleset_pair.indexed.contents.data(),
                                 test_ruleset_pair.indexed.contents.size());
  }

  void RunUntilIdle() { task_runner_->RunUntilIdle(); }

  void RunPendingTasksNTimes(size_t n) {
    while (n--)
      task_runner_->RunPendingTasks();
  }

  void AssertValidRulesetFileWithContents(
      base::File* file,
      const std::vector<uint8_t>& expected_contents) {
    ASSERT_TRUE(file->IsValid());
    ASSERT_EQ(expected_contents, ReadFileContents(file));
  }

  void AssertReadonlyRulesetFile(base::File* file) {
    const char kTest[] = "t";
    ASSERT_TRUE(file->IsValid());
    ASSERT_EQ(-1, file->Write(0, kTest, sizeof(kTest)));
  }

  void AssertNoPendingTasks() { ASSERT_FALSE(task_runner_->HasPendingTask()); }

  PrefService* prefs() { return &pref_service_; }
  RulesetService* service() { return service_.get(); }
  MockRulesetDistributor* mock_distributor() { return mock_distributor_; }

  const TestRulesetPair& test_ruleset_1() const { return test_ruleset_1_; }
  const TestRulesetPair& test_ruleset_2() const { return test_ruleset_2_; }
  base::FilePath base_dir() const {
    return scoped_temp_dir_.path().AppendASCII("Rules").AppendASCII("Indexed");
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  TestingPrefServiceSimple pref_service_;

  TestRulesetCreator ruleset_creator_;
  TestRulesetPair test_ruleset_1_;
  TestRulesetPair test_ruleset_2_;

  base::ScopedTempDir scoped_temp_dir_;
  std::unique_ptr<RulesetService> service_;
  MockRulesetDistributor* mock_distributor_;  // Weak, owned by |service_|.

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilteringRulesetServiceTest);
};

TEST_F(SubresourceFilteringRulesetServiceTest, Startup_NoRulesetNotPublished) {
  EXPECT_EQ(0u, mock_distributor()->published_rulesets().size());
}

// It should not normally happen that Local State indicates that a usable
// version of the ruleset had been stored, yet the file is nowhere to be found,
// but ensure some sane behavior just in case.
TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_MissingRulesetNotPublished) {
  IndexedRulesetVersion current_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  // "Forget" to write ruleset data.
  current_version.SaveToPrefs(prefs());

  ResetService(CreateRulesetService());
  RunUntilIdle();
  EXPECT_EQ(0u, mock_distributor()->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_LegacyFormatRulesetNotPublished) {
  int legacy_format_version = IndexedRulesetVersion::CurrentFormatVersion() - 1;
  IndexedRulesetVersion legacy_version(kTestContentVersion1,
                                       legacy_format_version);
  ASSERT_TRUE(legacy_version.IsValid());
  legacy_version.SaveToPrefs(prefs());
  WritePreexistingRuleset(test_ruleset_1(), legacy_version);

  ResetService(CreateRulesetService());
  RunUntilIdle();
  EXPECT_EQ(0u, mock_distributor()->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_ExistingRulesetPublished) {
  IndexedRulesetVersion current_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  current_version.SaveToPrefs(prefs());
  WritePreexistingRuleset(test_ruleset_1(), current_version);

  ResetService(CreateRulesetService());
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_Published) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetWithEmptyVersion_NotPublished) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), std::string());
  RunUntilIdle();

  ASSERT_EQ(0u, mock_distributor()->published_rulesets().size());
  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(0u, new_distributor->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_Persisted) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
  EXPECT_EQ(IndexedRulesetVersion::CurrentFormatVersion(),
            stored_version.format_version);

  ResetService(CreateRulesetService());
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwice_SecondRulesetPrevails) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(), kTestContentVersion2);
  RunUntilIdle();

  // This verifies that the contents from the first version of the ruleset file
  // can still be read after it has been deprecated.
  ASSERT_EQ(2u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[1],
      test_ruleset_2().indexed.contents));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0],
      test_ruleset_2().indexed.contents));

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion2, stored_version.content_version);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceWithTheSameVersion_SecondIsIgnored) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  // For good measure, also violate the requirement that versions should
  // uniquely identify the contents.
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(), kTestContentVersion1);
  ASSERT_NO_FATAL_FAILURE(AssertNoPendingTasks());

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0],
      test_ruleset_1().indexed.contents));

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetSetShortlyBeforeDestruction_NoCrashes) {
  for (size_t num_tasks_inbetween = 0; num_tasks_inbetween < 5u;
       ++num_tasks_inbetween) {
    SCOPED_TRACE(::testing::Message() << "#Tasks: " << num_tasks_inbetween);

    IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                          kTestContentVersion1);
    RunPendingTasksNTimes(num_tasks_inbetween);
    ResetService();
    RunUntilIdle();

    EXPECT_TRUE(base::DeleteFile(base_dir(), true));
    ResetService(CreateRulesetService());
  }

  // Must pump out PostTaskWithReply tasks that are referencing the very same
  // task runner to avoid circular dependencies and leaks on shutdown.
  RunUntilIdle();
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceInQuickSuccession_SecondRulesetPrevails) {
  for (size_t num_tasks_inbetween = 0; num_tasks_inbetween < 5u;
       ++num_tasks_inbetween) {
    SCOPED_TRACE(::testing::Message() << "#Tasks: " << num_tasks_inbetween);

    IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                          kTestContentVersion1);
    RunPendingTasksNTimes(num_tasks_inbetween);

    IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(),
                                          kTestContentVersion2);
    RunUntilIdle();

    // Optionally permit a "hazardous" publication of either the old or new
    // version of the ruleset, but the last ruleset message must be the new one.
    ASSERT_LE(1u, mock_distributor()->published_rulesets().size());
    ASSERT_GE(2u, mock_distributor()->published_rulesets().size());
    if (mock_distributor()->published_rulesets().size() == 2) {
      base::File* file = &mock_distributor()->published_rulesets()[0];
      ASSERT_TRUE(file->IsValid());
      EXPECT_THAT(
          ReadFileContents(file),
          ::testing::AnyOf(::testing::Eq(test_ruleset_1().indexed.contents),
                           ::testing::Eq(test_ruleset_2().indexed.contents)));
    }
    ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
        &mock_distributor()->published_rulesets().back(),
        test_ruleset_2().indexed.contents));

    MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
    service()->RegisterDistributor(base::WrapUnique(new_distributor));
    ASSERT_EQ(1u, new_distributor->published_rulesets().size());
    ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
        &new_distributor->published_rulesets()[0],
        test_ruleset_2().indexed.contents));

    IndexedRulesetVersion stored_version;
    stored_version.ReadFromPrefs(prefs());
    EXPECT_EQ(kTestContentVersion2, stored_version.content_version);

    ResetService();
    RunUntilIdle();

    EXPECT_TRUE(base::DeleteFile(base_dir(), true));
    IndexedRulesetVersion().SaveToPrefs(prefs());
    ResetService(CreateRulesetService());
  }
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceForTheSameVersion_SuccessAtLeastOnce) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  ASSERT_LE(1u, mock_distributor()->published_rulesets().size());
  ASSERT_GE(2u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets().front(),
      test_ruleset_1().indexed.contents));
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets().back(),
      test_ruleset_1().indexed.contents));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest, RulesetIsReadonly) {
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(
      AssertReadonlyRulesetFile(&mock_distributor()->published_rulesets()[0]));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(
      AssertReadonlyRulesetFile(&new_distributor->published_rulesets()[0]));
}

}  // namespace subresource_filter
