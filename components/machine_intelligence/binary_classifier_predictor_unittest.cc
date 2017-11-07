// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/binary_classifier_predictor.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/machine_intelligence/fake_ranker_model_loader.h"
#include "components/machine_intelligence/proto/ranker_model.pb.h"
#include "components/machine_intelligence/ranker_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_intelligence {

using ::machine_intelligence::testing::FakeRankerModelLoader;

class BinaryClassifierPredictorTest : public ::testing::Test {
 public:
  std::unique_ptr<BinaryClassifierPredictor> InitPredictor(
      std::unique_ptr<RankerModel> ranker_model);

  // This model will return the value of |feature| as a prediction.
  GenericLogisticRegressionModel GetSimpleLogisticRegressionModel();

 protected:
  const std::string feature_ = "feature";
  const float threshold_ = 0.5;
};

std::unique_ptr<BinaryClassifierPredictor>
BinaryClassifierPredictorTest::InitPredictor(
    std::unique_ptr<RankerModel> ranker_model) {
  std::unique_ptr<BinaryClassifierPredictor> predictor(
      new BinaryClassifierPredictor());
  auto fake_model_loader = base::MakeUnique<FakeRankerModelLoader>(
      base::Bind(&BinaryClassifierPredictor::ValidateModel),
      base::Bind(&BinaryClassifierPredictor::OnModelAvailable,
                 base::Unretained(predictor.get())),
      std::move(ranker_model));
  predictor->LoadModel(std::move(fake_model_loader));
  return predictor;
}

GenericLogisticRegressionModel
BinaryClassifierPredictorTest::GetSimpleLogisticRegressionModel() {
  GenericLogisticRegressionModel lr_model;
  lr_model.set_bias(-0.5);
  lr_model.set_threshold(threshold_);
  (*lr_model.mutable_weights())[feature_].set_scalar(1.0);
  return lr_model;
}

// TODO(hamelphi): Test BinaryClassifierPredictor::Create.

TEST_F(BinaryClassifierPredictorTest, EmptyRankerModel) {
  auto ranker_model = base::MakeUnique<RankerModel>();
  auto predictor = InitPredictor(std::move(ranker_model));
  EXPECT_FALSE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_FALSE(predictor->Predict(ranker_example, &bool_response));
  float float_response;
  EXPECT_FALSE(predictor->PredictScore(ranker_example, &float_response));
}

TEST_F(BinaryClassifierPredictorTest, NoInferenceModuleForModel) {
  auto ranker_model = base::MakeUnique<RankerModel>();
  // TranslateRankerModel does not have an inference module. Validation will
  // fail.
  ranker_model->mutable_proto()
      ->mutable_translate()
      ->mutable_translate_logistic_regression_model()
      ->set_bias(1);
  auto predictor = InitPredictor(std::move(ranker_model));
  EXPECT_FALSE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_FALSE(predictor->Predict(ranker_example, &bool_response));
  float float_response;
  EXPECT_FALSE(predictor->PredictScore(ranker_example, &float_response));
}

TEST_F(BinaryClassifierPredictorTest, GenericLogisticRegressionModel) {
  auto ranker_model = base::MakeUnique<RankerModel>();
  *ranker_model->mutable_proto()->mutable_logistic_regression() =
      GetSimpleLogisticRegressionModel();
  auto predictor = InitPredictor(std::move(ranker_model));
  EXPECT_TRUE(predictor->IsReady());

  RankerExample ranker_example;
  auto& features = *ranker_example.mutable_features();
  features[feature_].set_bool_value(true);
  bool bool_response;
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_TRUE(bool_response);
  float float_response;
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_GT(float_response, threshold_);

  features[feature_].set_bool_value(false);
  EXPECT_TRUE(predictor->Predict(ranker_example, &bool_response));
  EXPECT_FALSE(bool_response);
  EXPECT_TRUE(predictor->PredictScore(ranker_example, &float_response));
  EXPECT_LT(float_response, threshold_);
}

}  // namespace machine_intelligence
