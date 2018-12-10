// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/assist_ranker/nn_classifier.h"
#include "base/logging.h"
#include "components/assist_ranker/nn_classifier_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace assist_ranker {
namespace nn_classifier {
namespace {

using ::google::protobuf::RepeatedFieldBackInserter;
using ::std::copy;
using ::std::vector;

TEST(NNClassifierTest, XorTest) {
  // Creates a NN with a single hidden layer of 5 units that solves XOR.
  // Creates a DNNClassifier model containing the trained biases and weights.
  const NNClassifierModel model = CreateModel(
      // Hidden biases.
      {{-0.45737201, 0.2009858, 1.02393341, -1.72199488, -0.54427308}},
      // Hidden weights.
      {{2.21626472, -0.08185583, -0.7542417, 1.97279537, 0.62363654},
       {-1.71283901, 2.0275352, -1.14731216, 1.56915629, 0.49627137}},
      // Logits biases.
      {-1.27781141},
      // Logits weights.
      {{2.8636384}, {1.84202337}, {-1.76555872}, {-2.96390629}, {-1.00649774}});

  ASSERT_TRUE(Validate(model));
  EXPECT_TRUE(CheckInference(model, {0, 0}, {-2.7154054}));
  EXPECT_TRUE(CheckInference(model, {0, 1}, {2.8271765}));
  EXPECT_TRUE(CheckInference(model, {1, 0}, {2.6790769}));
  EXPECT_TRUE(CheckInference(model, {1, 1}, {-3.1652793}));
}

TEST(NNClassifierTest, ValidateNNClassifierModel) {
  // Empty model.
  NNClassifierModel model;
  EXPECT_FALSE(Validate(model));

  // Valid model.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}, {0}});
  EXPECT_TRUE(Validate(model));

  // Too few hidden layer biases.
  model = CreateModel({0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}, {0}});
  EXPECT_FALSE(Validate(model));

  // Too few hidden layer weights.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0}}, {0}, {{0}, {0}, {0}});
  EXPECT_FALSE(Validate(model));

  // Too few logits weights.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {0}, {{0}, {0}});
  EXPECT_FALSE(Validate(model));

  // Logits biases empty.
  model = CreateModel({0, 0, 0}, {{0, 0, 0}, {0, 0, 0}}, {}, {{0}, {0}, {0}});
  EXPECT_FALSE(Validate(model));
}

}  // namespace
}  // namespace nn_classifier
}  // namespace assist_ranker
