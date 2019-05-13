// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
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

    PartiallyPopulateConfig(&config_);
    // Even if empty, the setting of the oneof |predictor_config| in
    // RecurrenceRankerConfigProto is used to determine which predictor is
    // constructed.
    config_.mutable_fake_predictor();

    ranker_ =
        std::make_unique<RecurrenceRanker>(ranker_filepath_, config_, false);
    Wait();
  }

  void Wait() { scoped_task_environment_.RunUntilIdle(); }

  RecurrenceRankerProto MakeTestingProto() {
    RecurrenceRankerProto proto;
    proto.set_config_hash(base::PersistentHash(config_.SerializeAsString()));

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

    // Make FakePredictor counts.
    auto* counts =
        proto.mutable_predictor()->mutable_fake_predictor()->mutable_counts();
    (*counts)[0u] = 1.0f;
    (*counts)[1u] = 2.0f;
    (*counts)[2u] = 1.0f;

    return proto;
  }

  RecurrenceRankerConfigProto config_;
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
      base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<RecurrenceRanker> ranker_;
  base::FilePath ranker_filepath_;
};

TEST_F(RecurrenceRankerTest, Record) {
  ranker_->Record("A");
  ranker_->Record("B");
  ranker_->Record("B");

  EXPECT_THAT(ranker_->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                    Pair("B", FloatEq(2.0f))));
}

TEST_F(RecurrenceRankerTest, RenameTarget) {
  ranker_->Record("A");
  ranker_->Record("B");
  ranker_->Record("B");
  ranker_->RenameTarget("B", "A");

  EXPECT_THAT(ranker_->Rank(), ElementsAre(Pair("A", FloatEq(2.0f))));
}

TEST_F(RecurrenceRankerTest, RemoveTarget) {
  ranker_->Record("A");
  ranker_->Record("B");
  ranker_->Record("B");
  ranker_->RemoveTarget("A");

  EXPECT_THAT(ranker_->Rank(), ElementsAre(Pair("B", FloatEq(2.0f))));
}

TEST_F(RecurrenceRankerTest, ComplexRecordAndRank) {
  ranker_->Record("A");
  ranker_->Record("B");
  ranker_->Record("C");
  ranker_->Record("B");
  ranker_->RenameTarget("D", "C");
  ranker_->RemoveTarget("F");
  ranker_->RenameTarget("C", "F");
  ranker_->RemoveTarget("A");
  ranker_->RenameTarget("C", "F");
  ranker_->Record("A");

  EXPECT_THAT(ranker_->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                    Pair("B", FloatEq(2.0f)),
                                                    Pair("F", FloatEq(1.0f))));
}

TEST_F(RecurrenceRankerTest, RankTopN) {
  const std::vector<std::string> targets = {"B", "A", "A", "B", "C",
                                            "B", "D", "C", "A", "A"};
  for (auto target : targets)
    ranker_->Record(target);

  EXPECT_THAT(ranker_->RankTopN(2),
              ElementsAre(Pair("A", FloatEq(4.0f)), Pair("B", FloatEq(3.0f))));
  EXPECT_THAT(ranker_->RankTopN(100),
              ElementsAre(Pair("A", FloatEq(4.0f)), Pair("B", FloatEq(3.0f)),
                          Pair("C", FloatEq(2.0f)), Pair("D", FloatEq(1.0f))));
}

TEST_F(RecurrenceRankerTest, LoadFromDisk) {
  // Set up a temp dir.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filepath = temp_dir.GetPath().AppendASCII("recurrence_ranker");

  // Serialise a testing proto.
  RecurrenceRankerProto proto = MakeTestingProto();
  const std::string proto_str = proto.SerializeAsString();
  EXPECT_NE(base::WriteFile(filepath, proto_str.c_str(), proto_str.size()), -1);

  // Make a ranker.
  RecurrenceRanker ranker(filepath, config_, false);

  // Check that the file loading is executed in non-blocking way.
  EXPECT_FALSE(ranker.load_from_disk_completed_);
  Wait();
  EXPECT_TRUE(ranker.load_from_disk_completed_);

  // Check predictor is loaded correctly.
  EXPECT_THAT(ranker.Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                  Pair("B", FloatEq(2.0f)),
                                                  Pair("C", FloatEq(1.0f))));
}

TEST_F(RecurrenceRankerTest, InitializeIfNoFileExists) {
  // Set up a temp dir with an empty file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filepath =
      temp_dir.GetPath().AppendASCII("recurrence_ranker_invalid");

  RecurrenceRanker ranker(filepath, config_, false);
  Wait();
  EXPECT_TRUE(ranker_->load_from_disk_completed_);
  EXPECT_TRUE(ranker.Rank().empty());
}

TEST_F(RecurrenceRankerTest, SaveToDisk) {
  // Sanity checks
  ASSERT_TRUE(ranker_->load_from_disk_completed_);
  EXPECT_TRUE(ranker_->Rank().empty());

  // Check the ranker file is not created.
  EXPECT_FALSE(base::PathExists(ranker_filepath_));

  // Make the ranker do a save.
  ranker_->Record("A");
  ranker_->Record("B");
  ranker_->Record("B");
  ranker_->ForceSaveOnNextUpdateForTesting();
  ranker_->Record("C");
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
}

TEST_F(RecurrenceRankerTest, SavedRankerRejectedIfConfigMismatched) {
  // Make the first ranker do a save.
  ranker_->ForceSaveOnNextUpdateForTesting();
  ranker_->Record("A");
  Wait();

  // Construct a second ranker with a slightly different config.
  RecurrenceRankerConfigProto other_config;
  PartiallyPopulateConfig(&other_config);
  other_config.mutable_fake_predictor();
  other_config.set_min_seconds_between_saves(
      config_.min_seconds_between_saves() + 1);

  RecurrenceRanker other_ranker(ranker_filepath_, other_config, false);
  Wait();

  // Expect that the second ranker doesn't return any rankings, because it
  // rejected the saved proto as being made by a different config.
  EXPECT_TRUE(other_ranker.load_from_disk_completed_);
  EXPECT_TRUE(other_ranker.Rank().empty());
  // For comparison:
  EXPECT_THAT(ranker_->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f))));
}

TEST_F(RecurrenceRankerTest, EphemeralUsersUseDefaultPredictor) {
  RecurrenceRanker ephemeral_ranker(ranker_filepath_, config_, true);
  Wait();
  EXPECT_THAT(ephemeral_ranker.GetPredictorNameForTesting(),
              StrEq(DefaultPredictor::kPredictorName));
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
}

}  // namespace app_list
