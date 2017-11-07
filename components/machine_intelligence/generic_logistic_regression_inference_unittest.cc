// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/generic_logistic_regression_inference.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace machine_intelligence {

class GenericLogisticRegressionInferenceTest : public testing::Test {
 protected:
  GenericLogisticRegressionModel GetProto() {
    GenericLogisticRegressionModel proto;
    proto.set_bias(bias_);
    proto.set_threshold(threshold_);

    auto& weights = *proto.mutable_weights();
    weights[scalar1_name_].set_scalar(scalar1_weight_);
    weights[scalar2_name_].set_scalar(scalar2_weight_);
    weights[scalar3_name_].set_scalar(scalar3_weight_);
    auto* one_hot_feat = weights[one_hot_name_].mutable_one_hot();
    one_hot_feat->set_default_weight(one_hot_default_weight_);
    (*one_hot_feat->mutable_weights())[one_hot_elem1_name_] = elem1_weight_;
    (*one_hot_feat->mutable_weights())[one_hot_elem2_name_] = elem2_weight_;
    (*one_hot_feat->mutable_weights())[one_hot_elem3_name_] = elem3_weight_;
    return proto;
  }

  const std::string scalar1_name_ = "scalar_feature1";
  const std::string scalar2_name_ = "scalar_feature2";
  const std::string scalar3_name_ = "scalar_feature3";
  const std::string one_hot_name_ = "one_hot_feature";
  const std::string one_hot_elem1_name_ = "elem1";
  const std::string one_hot_elem2_name_ = "elem2";
  const std::string one_hot_elem3_name_ = "elem3";
  const float bias_ = 1.5f;
  const float threshold_ = 0.6f;
  const float scalar1_weight_ = 0.8f;
  const float scalar2_weight_ = -2.4f;
  const float scalar3_weight_ = 0.01f;
  const float elem1_weight_ = -1.0f;
  const float elem2_weight_ = 5.0f;
  const float elem3_weight_ = -1.5f;
  const float one_hot_default_weight_ = 10.0f;
  const float epsilon_ = 0.001f;
};

TEST_F(GenericLogisticRegressionInferenceTest, BaseTest) {
  auto predictor = GenericLogisticRegressionInference(GetProto());

  RankerExample example;
  auto& features = *example.mutable_features();
  features[scalar1_name_].set_bool_value(true);
  features[scalar2_name_].set_int32_value(42);
  features[scalar3_name_].set_float_value(0.666f);
  features[one_hot_name_].set_string_value(one_hot_elem1_name_);

  float score = predictor.PredictScore(example);
  float expected_score =
      Sigmoid(bias_ + 1.0f * scalar1_weight_ + 42.0f * scalar2_weight_ +
              0.666f * scalar3_weight_ + elem1_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
  EXPECT_EQ(expected_score >= threshold_, predictor.Predict(example));
}

TEST_F(GenericLogisticRegressionInferenceTest, UnknownElement) {
  RankerExample example;
  auto& features = *example.mutable_features();
  features[one_hot_name_].set_string_value("Unknown element");

  auto predictor = GenericLogisticRegressionInference(GetProto());
  float score = predictor.PredictScore(example);
  float expected_score = Sigmoid(bias_ + one_hot_default_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
}

TEST_F(GenericLogisticRegressionInferenceTest, MissingFeatures) {
  RankerExample example;

  auto predictor = GenericLogisticRegressionInference(GetProto());
  float score = predictor.PredictScore(example);
  // Missing features will use default weights for one_hot features and drop
  // scalar features.
  float expected_score = Sigmoid(bias_ + one_hot_default_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
}

TEST_F(GenericLogisticRegressionInferenceTest, UnknownFeatures) {
  RankerExample example;
  auto& features = *example.mutable_features();
  features["foo1"].set_bool_value(true);
  features["foo2"].set_int32_value(42);
  features["foo3"].set_float_value(0.666f);
  features["foo4"].set_string_value(one_hot_elem1_name_);
  // All features except this one will be ignored.
  features[one_hot_name_].set_string_value(one_hot_elem2_name_);

  auto predictor = GenericLogisticRegressionInference(GetProto());
  float score = predictor.PredictScore(example);
  // Unknown features will be ignored.
  float expected_score = Sigmoid(bias_ + elem2_weight_);
  EXPECT_NEAR(expected_score, score, epsilon_);
}

TEST_F(GenericLogisticRegressionInferenceTest, Threshold) {
  // In this test, we calculate the score for a given example and set the model
  // threshold to this value. We then add a feature to the example that should
  // tip the score slightly on either side of the treshold and verify that the
  // decision is as expected.

  auto proto = GetProto();
  auto threshold_calculator = GenericLogisticRegressionInference(proto);

  RankerExample example;
  auto& features = *example.mutable_features();
  features[scalar1_name_].set_bool_value(true);
  features[scalar2_name_].set_int32_value(2);
  features[one_hot_name_].set_string_value(one_hot_elem1_name_);

  float threshold = threshold_calculator.PredictScore(example);
  proto.set_threshold(threshold);

  // Setting the model with the calculated threshold.
  auto predictor = GenericLogisticRegressionInference(proto);

  // Adding small positive contribution from scalar3 to tip the decision the
  // positive side of the threshold.
  features[scalar3_name_].set_float_value(0.01f);
  float score = predictor.PredictScore(example);
  // The score is now greater than, but still near the threshold. The
  // decision should be positive.
  EXPECT_LT(threshold, score);
  EXPECT_NEAR(threshold, score, epsilon_);
  EXPECT_TRUE(predictor.Predict(example));

  // A small negative contribution from scalar3 should tip the decision the
  // other way.
  features[scalar3_name_].set_float_value(-0.01f);
  score = predictor.PredictScore(example);
  EXPECT_GT(threshold, score);
  EXPECT_NEAR(threshold, score, epsilon_);
  EXPECT_FALSE(predictor.Predict(example));
}

TEST_F(GenericLogisticRegressionInferenceTest, NoThreshold) {
  auto proto = GetProto();
  // When no threshold is specified, we use the default of 0.5.
  proto.clear_threshold();
  auto predictor = GenericLogisticRegressionInference(proto);

  RankerExample example;
  auto& features = *example.mutable_features();
  // one_hot_elem3 exactly balances the bias, so we expect the pre-sigmoid score
  // to be zero, and the post-sigmoid score to be 0.5 if this is the only active
  // feature.
  features[one_hot_name_].set_string_value(one_hot_elem3_name_);
  float score = predictor.PredictScore(example);
  EXPECT_NEAR(0.5f, score, epsilon_);

  // Adding small contribution from scalar3 to tip the decision on one side or
  // the other of the threshold.
  features[scalar3_name_].set_float_value(0.01f);
  score = predictor.PredictScore(example);
  // The score is now greater than, but still near 0.5. The decision should be
  // positive.
  EXPECT_LT(0.5f, score);
  EXPECT_NEAR(0.5f, score, epsilon_);
  EXPECT_TRUE(predictor.Predict(example));

  features[scalar3_name_].set_float_value(-0.01f);
  score = predictor.PredictScore(example);
  // The score is now lower than, but near 0.5. The decision should be
  // negative.
  EXPECT_GT(0.5f, score);
  EXPECT_NEAR(0.5f, score, epsilon_);
  EXPECT_FALSE(predictor.Predict(example));
}

}  // namespace machine_intelligence
