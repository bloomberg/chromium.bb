// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::FloatEq;
using testing::Pair;
using testing::StrEq;
using testing::UnorderedElementsAre;

namespace app_list {

namespace {

// For convenience, sets all fields of a config proto except for the predictor.
void PartiallyPopulateConfig(RecurrenceRankerConfigProto* config) {
  config->set_target_limit(100u);
  config->set_target_decay(0.8f);
  config->set_condition_limit(101u);
  config->set_condition_decay(0.81f);
  config->set_min_seconds_between_saves(5);
}

}  // namespace

class RecurrenceRankerTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ranker_filepath_ = temp_dir_.GetPath().AppendASCII("recurrence_ranker");
    Wait();
  }

  void Wait() { scoped_task_environment_.RunUntilIdle(); }

  // Returns the config for a ranker with a fake predictor.
  RecurrenceRankerConfigProto MakeSimpleConfig() {
    RecurrenceRankerConfigProto config;
    PartiallyPopulateConfig(&config);
    // Even if empty, the setting of the oneof |predictor_config| in
    // RecurrenceRankerConfigProto is used to determine which predictor is
    // constructed.
    config.mutable_fake_predictor();
    return config;
  }

  // Returns a ranker using a fake predictor.
  std::unique_ptr<RecurrenceRanker> MakeSimpleRanker() {
    auto ranker = std::make_unique<RecurrenceRanker>(ranker_filepath_,
                                                     MakeSimpleConfig(), false);
    Wait();
    return ranker;
  }

  RecurrenceRankerProto MakeTestingProto() {
    RecurrenceRankerProto proto;
    proto.set_config_hash(
        base::PersistentHash(MakeSimpleConfig().SerializeAsString()));

    // Make target frecency store.
    auto* targets = proto.mutable_targets();
    targets->set_value_limit(100u);
    targets->set_decay_coeff(0.8f);
    targets->set_num_updates(4);
    targets->set_next_id(3);
    auto* target_values = targets->mutable_values();

    FrecencyStoreProto::ValueData value_data;
    value_data.set_id(0u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(1);
    (*target_values)["A"] = value_data;

    value_data = FrecencyStoreProto::ValueData();
    value_data.set_id(1u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(3);
    (*target_values)["B"] = value_data;

    value_data = FrecencyStoreProto::ValueData();
    value_data.set_id(2u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(4);
    (*target_values)["C"] = value_data;

    // Make empty conditions frecency store.
    auto* conditions = proto.mutable_conditions();
    conditions->set_value_limit(0u);
    conditions->set_decay_coeff(0.0f);
    conditions->set_num_updates(0);
    conditions->set_next_id(0);
    conditions->mutable_values();

    // Make FakePredictor counts.
    auto* counts =
        proto.mutable_predictor()->mutable_fake_predictor()->mutable_counts();
    (*counts)[0u] = 1.0f;
    (*counts)[1u] = 2.0f;
    (*counts)[2u] = 1.0f;

    return proto;
  }

  void ExpectErrors(bool fresh_model_created, bool using_fake_predictor) {
    histogram_tester_.ExpectTotalCount("RecurrenceRanker.UsageError", 0);

    // If a model doesn't already exist, a read error is logged.
    if (fresh_model_created) {
      histogram_tester_.ExpectUniqueSample(
          "RecurrenceRanker.SerializationError",
          SerializationError::kModelReadError, 1);
    } else {
      histogram_tester_.ExpectTotalCount("RecurrenceRanker.SerializationError",
                                         0);
    }

    // Initialising with the fake predictor logs an UMA error, because it should
    // be used only in tests and not in production.
    if (using_fake_predictor) {
      histogram_tester_.ExpectUniqueSample(
          "RecurrenceRanker.ConfigurationError",
          ConfigurationError::kFakePredictorUsed, 1);
    } else {
      histogram_tester_.ExpectTotalCount("RecurrenceRanker.ConfigurationError",
                                         0);
    }
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
      base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  base::FilePath ranker_filepath_;
};

TEST_F(RecurrenceRankerTest, Record) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");

  EXPECT_THAT(ranker->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                   Pair("B", FloatEq(2.0f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, RenameTarget) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");
  ranker->RenameTarget("B", "A");

  EXPECT_THAT(ranker->Rank(), ElementsAre(Pair("A", FloatEq(2.0f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, RemoveTarget) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");
  ranker->RemoveTarget("A");

  EXPECT_THAT(ranker->Rank(), ElementsAre(Pair("B", FloatEq(2.0f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, ComplexRecordAndRank) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("C");
  ranker->Record("B");
  ranker->RenameTarget("D", "C");
  ranker->RemoveTarget("F");
  ranker->RenameTarget("C", "F");
  ranker->RemoveTarget("A");
  ranker->RenameTarget("C", "F");
  ranker->Record("A");

  EXPECT_THAT(ranker->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                   Pair("B", FloatEq(2.0f)),
                                                   Pair("F", FloatEq(1.0f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, RankTopN) {
  auto ranker = MakeSimpleRanker();

  const std::vector<std::string> targets = {"B", "A", "A", "B", "C",
                                            "B", "D", "C", "A", "A"};
  for (auto target : targets)
    ranker->Record(target);

  EXPECT_THAT(ranker->RankTopN(2),
              ElementsAre(Pair("A", FloatEq(4.0f)), Pair("B", FloatEq(3.0f))));
  EXPECT_THAT(ranker->RankTopN(100),
              ElementsAre(Pair("A", FloatEq(4.0f)), Pair("B", FloatEq(3.0f)),
                          Pair("C", FloatEq(2.0f)), Pair("D", FloatEq(1.0f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, LoadFromDisk) {
  // Serialise a testing proto.
  RecurrenceRankerProto proto = MakeTestingProto();
  const std::string proto_str = proto.SerializeAsString();
  EXPECT_NE(
      base::WriteFile(ranker_filepath_, proto_str.c_str(), proto_str.size()),
      -1);

  // Make a ranker.
  RecurrenceRanker ranker(ranker_filepath_, MakeSimpleConfig(), false);

  // Check that the file loading is executed in non-blocking way.
  EXPECT_FALSE(ranker.load_from_disk_completed_);
  Wait();
  EXPECT_TRUE(ranker.load_from_disk_completed_);

  // Check predictor is loaded correctly.
  EXPECT_THAT(ranker.Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                  Pair("B", FloatEq(2.0f)),
                                                  Pair("C", FloatEq(1.0f))));
  ExpectErrors(/*fresh_model_created = */ false,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, InitializeIfNoFileExists) {
  // Set up a temp dir with no saved ranker.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filepath =
      temp_dir.GetPath().AppendASCII("recurrence_ranker_invalid");

  RecurrenceRanker ranker(filepath, MakeSimpleConfig(), false);
  Wait();

  EXPECT_TRUE(ranker.load_from_disk_completed_);
  EXPECT_TRUE(ranker.Rank().empty());

  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, SaveToDisk) {
  auto ranker = MakeSimpleRanker();

  // Sanity checks
  ASSERT_TRUE(ranker->load_from_disk_completed_);
  EXPECT_TRUE(ranker->Rank().empty());

  // Check the ranker file is not created.
  EXPECT_FALSE(base::PathExists(ranker_filepath_));

  // Make the ranker do a save.
  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");
  ranker->ForceSaveOnNextUpdateForTesting();
  ranker->Record("C");
  Wait();

  // Check the ranker file is created.
  EXPECT_TRUE(base::PathExists(ranker_filepath_));

  // Parse the content of the file.
  std::string str_written;
  EXPECT_TRUE(base::ReadFileToString(ranker_filepath_, &str_written));
  RecurrenceRankerProto proto_written;
  EXPECT_TRUE(proto_written.ParseFromString(str_written));

  // Expect the content to be proto_.
  EXPECT_TRUE(EquivToProtoLite(proto_written, MakeTestingProto()));

  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, SavedRankerRejectedIfConfigMismatched) {
  auto ranker = MakeSimpleRanker();

  // Make the first ranker do a save.
  ranker->ForceSaveOnNextUpdateForTesting();
  ranker->Record("A");
  Wait();

  // Construct a second ranker with a slightly different config.
  RecurrenceRankerConfigProto other_config;
  PartiallyPopulateConfig(&other_config);
  other_config.mutable_fake_predictor();
  other_config.set_min_seconds_between_saves(1234);

  RecurrenceRanker other_ranker(ranker_filepath_, other_config, false);
  Wait();

  // Expect that the second ranker doesn't return any rankings, because it
  // rejected the saved proto as being made by a different config.
  EXPECT_TRUE(other_ranker.load_from_disk_completed_);
  EXPECT_TRUE(other_ranker.Rank().empty());
  // For comparison:
  EXPECT_THAT(ranker->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f))));
  // Should also log an error to UMA.
  histogram_tester_.ExpectBucketCount("RecurrenceRanker.ConfigurationError",
                                      ConfigurationError::kHashMismatch, 1);
}

TEST_F(RecurrenceRankerTest, EphemeralUsersUseDefaultPredictor) {
  RecurrenceRanker ephemeral_ranker(ranker_filepath_, MakeSimpleConfig(), true);
  Wait();
  EXPECT_THAT(ephemeral_ranker.GetPredictorNameForTesting(),
              StrEq(DefaultPredictor::kPredictorName));
  ExpectErrors(/*fresh_model_created = */ false,
               /*using_fake_predictor = */ false);
}

TEST_F(RecurrenceRankerTest, IntegrationWithDefaultPredictor) {
  RecurrenceRankerConfigProto config;
  PartiallyPopulateConfig(&config);
  config.mutable_default_predictor();

  RecurrenceRanker ranker(ranker_filepath_, config, false);
  Wait();

  ranker.Record("A");
  ranker.Record("A");
  ranker.Record("B");
  ranker.Record("C");

  EXPECT_THAT(ranker.Rank(), UnorderedElementsAre(Pair("A", FloatEq(0.2304f)),
                                                  Pair("B", FloatEq(0.16f)),
                                                  Pair("C", FloatEq(0.2f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ false);
}

TEST_F(RecurrenceRankerTest, IntegrationWithZeroStateFrecencyPredictor) {
  RecurrenceRankerConfigProto config;
  PartiallyPopulateConfig(&config);
  auto* predictor = config.mutable_zero_state_frecency_predictor();
  predictor->set_decay_coeff(0.5f);

  RecurrenceRanker ranker(ranker_filepath_, config, false);
  Wait();

  ranker.Record("A");
  ranker.Record("A");
  ranker.Record("D");
  ranker.Record("C");
  ranker.Record("E");
  ranker.RenameTarget("D", "B");
  ranker.RemoveTarget("E");
  ranker.RenameTarget("E", "A");

  EXPECT_THAT(ranker.Rank(), UnorderedElementsAre(Pair("A", FloatEq(0.09375f)),
                                                  Pair("B", FloatEq(0.125f)),
                                                  Pair("C", FloatEq(0.25f))));
  ExpectErrors(/*fresh_model_created = */ true,
               /*using_fake_predictor = */ false);
}

}  // namespace app_list
