// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::FloatEq;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace app_list {

class ZeroStateFrecencyPredictorTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    config_.set_decay_coeff(0.5f);
    predictor_ = std::make_unique<ZeroStateFrecencyPredictor>(config_);
  }

  ZeroStateFrecencyPredictorConfig config_;
  std::unique_ptr<ZeroStateFrecencyPredictor> predictor_;
};

TEST_F(ZeroStateFrecencyPredictorTest, RankWithNoTargets) {
  EXPECT_TRUE(predictor_->Rank().empty());
}

TEST_F(ZeroStateFrecencyPredictorTest, RecordAndRankSimple) {
  predictor_->Train(2u);
  predictor_->Train(4u);
  predictor_->Train(6u);

  EXPECT_THAT(
      predictor_->Rank(),
      UnorderedElementsAre(Pair(2u, FloatEq(0.125f)), Pair(4u, FloatEq(0.25f)),
                           Pair(6u, FloatEq(0.5f))));
}

TEST_F(ZeroStateFrecencyPredictorTest, RecordAndRankComplex) {
  predictor_->Train(2u);
  predictor_->Train(4u);
  predictor_->Train(6u);
  predictor_->Train(4u);
  predictor_->Train(2u);

  // Ranks should be deterministic.
  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT(predictor_->Rank(),
                UnorderedElementsAre(Pair(2u, FloatEq(0.53125f)),
                                     Pair(4u, FloatEq(0.3125f)),
                                     Pair(6u, FloatEq(0.125f))));
  }
}

TEST_F(ZeroStateFrecencyPredictorTest, ToAndFromProto) {
  predictor_->Train(1u);
  predictor_->Train(3u);
  predictor_->Train(5u);

  RecurrencePredictorProto proto;
  predictor_->ToProto(&proto);

  ZeroStateFrecencyPredictor new_predictor(config_);
  new_predictor.FromProto(proto);

  EXPECT_TRUE(proto.has_zero_state_frecency_predictor());
  EXPECT_EQ(proto.zero_state_frecency_predictor().num_updates(), 3u);
  EXPECT_EQ(predictor_->Rank(), new_predictor.Rank());
}

}  // namespace app_list
