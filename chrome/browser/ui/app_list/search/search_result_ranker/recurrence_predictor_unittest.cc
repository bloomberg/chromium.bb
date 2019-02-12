// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::FloatEq;
using testing::IsSupersetOf;
using testing::NiceMock;
using testing::Pair;
using testing::Return;
using testing::StrEq;
using testing::UnorderedElementsAre;

namespace app_list {

class ZeroStateFrecencyPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    config_.set_target_limit(100u);
    config_.set_decay_coeff(0.5f);
    predictor_ = std::make_unique<ZeroStateFrecencyPredictor>(config_);
  }

  ZeroStateFrecencyPredictorConfig config_;
  std::unique_ptr<ZeroStateFrecencyPredictor> predictor_;
};

TEST_F(ZeroStateFrecencyPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank("").empty());
}

TEST_F(ZeroStateFrecencyPredictorTest, RecordAndRank) {
  predictor_->Train("A", "");
  predictor_->Train("B", "");
  predictor_->Train("C", "");

  EXPECT_THAT(predictor_->Rank(""),
              UnorderedElementsAre(Pair("A", FloatEq(0.125f)),
                                   Pair("B", FloatEq(0.25f)),
                                   Pair("C", FloatEq(0.5f))));
}

TEST_F(ZeroStateFrecencyPredictorTest, Rename) {
  predictor_->Train("A", "");
  predictor_->Train("B", "");
  predictor_->Train("B", "");
  predictor_->Rename("B", "A");

  EXPECT_THAT(predictor_->Rank(""),
              UnorderedElementsAre(Pair("A", FloatEq(0.75f))));
}

TEST_F(ZeroStateFrecencyPredictorTest, RenameNonexistentTarget) {
  predictor_->Train("A", "");
  predictor_->Rename("B", "C");

  EXPECT_THAT(predictor_->Rank(""),
              UnorderedElementsAre(Pair("A", FloatEq(0.5f))));
}

TEST_F(ZeroStateFrecencyPredictorTest, Remove) {
  predictor_->Train("A", "");
  predictor_->Train("B", "");
  predictor_->Remove("B");

  EXPECT_THAT(predictor_->Rank(""),
              UnorderedElementsAre(Pair("A", FloatEq(0.25f))));
}

TEST_F(ZeroStateFrecencyPredictorTest, RemoveNonexistentTarget) {
  predictor_->Train("A", "");
  predictor_->Remove("B");

  EXPECT_THAT(predictor_->Rank(""),
              UnorderedElementsAre(Pair("A", FloatEq(0.5f))));
}

TEST_F(ZeroStateFrecencyPredictorTest, TargetLimitExceeded) {
  ZeroStateFrecencyPredictorConfig config;
  config.set_target_limit(5u);
  config.set_decay_coeff(0.9999f);
  ZeroStateFrecencyPredictor predictor(config);

  // Insert many more targets than the target limit.
  for (int i = 0; i < 50; i++) {
    predictor.Train(std::to_string(i), "");
  }

  // Check that some of the values have been deleted, and the most recent ones
  // remain. We check loose bounds on these requirements, to prevent the test
  // from being tied to implementation details of the |FrecencyStore| cleanup
  // logic. See |FrecencyStoreTest::CleanupOnOverflow| for a corresponding, more
  // precise test.
  auto ranks = predictor.Rank("");
  EXPECT_LE(ranks.size(), 10ul);
  EXPECT_THAT(
      ranks, testing::IsSupersetOf({Pair("45", _), Pair("46", _), Pair("47", _),
                                    Pair("48", _), Pair("49", _)}));
}

TEST_F(ZeroStateFrecencyPredictorTest, ToAndFromProto) {
  predictor_->Train("A", "");
  predictor_->Train("B", "");
  predictor_->Train("C", "");

  const auto ranks = predictor_->Rank("");

  RecurrencePredictorProto proto;
  predictor_->ToProto(&proto);
  predictor_->FromProto(proto);

  EXPECT_EQ(ranks, predictor_->Rank(""));
}

}  // namespace app_list
