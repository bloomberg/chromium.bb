// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_impl.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest : public testing::Test {
 public:
  LearningTaskControllerImplTest()
      : predicted_target_(123), not_predicted_target_(456) {
    // Don't require too many training examples per report.
    task_.min_data_set_size = 4;

    controller_ = std::make_unique<LearningTaskControllerImpl>(task_);
    controller_->SetTrainingCBForTesting(base::BindRepeating(
        &LearningTaskControllerImplTest::OnTrain, base::Unretained(this)));
    controller_->SetAccuracyReportingCBForTesting(base::BindRepeating(
        &LearningTaskControllerImplTest::OnAccuracy, base::Unretained(this)));
  }

  // Model that always predicts a constant.
  class FakeModel : public Model {
   public:
    FakeModel(TargetValue target) : target_(target) {}

    // Model
    TargetDistribution PredictDistribution(
        const FeatureVector& features) override {
      TargetDistribution dist;
      dist += target_;
      return dist;
    }

   private:
    // The value we predict.
    TargetValue target_;
  };

  void OnTrain(TrainingData training_data, TrainedModelCB model_cb) {
    num_models_++;
    std::move(model_cb).Run(std::make_unique<FakeModel>(predicted_target_));
  }

  void OnAccuracy(const LearningTask& task, bool is_correct) {
    num_reported_++;
    if (is_correct)
      num_correct_++;
  }

  // Number of models that we trained.
  int num_models_ = 0;

  // Results reported via OnAccuracy.
  int num_reported_ = 0;
  int num_correct_ = 0;

  // Two distinct targets.
  TargetValue predicted_target_;
  TargetValue not_predicted_target_;

  LearningTask task_;
  std::unique_ptr<LearningTaskControllerImpl> controller_;
};

TEST_F(LearningTaskControllerImplTest, AddingExamplesTrainsModelAndReports) {
  TrainingExample example;

  // Adding the first n-1 examples shouldn't cause it to train a model.
  for (size_t i = 0; i < task_.min_data_set_size - 1; i++)
    controller_->AddExample(example);
  EXPECT_EQ(num_models_, 0);

  // Adding one more example should train a model.
  controller_->AddExample(example);
  EXPECT_EQ(num_models_, 1);

  // No results should be reported yet.
  EXPECT_EQ(num_reported_, 0);
  EXPECT_EQ(num_correct_, 0);

  // Adding one more example should report results.
  example.target_value = predicted_target_;
  controller_->AddExample(example);
  EXPECT_EQ(num_models_, 1);
  EXPECT_EQ(num_reported_, 1);
  EXPECT_EQ(num_correct_, 1);

  // Adding a value that doesn't match should report one more attempt.
  example.target_value = not_predicted_target_;
  controller_->AddExample(example);
  EXPECT_EQ(num_models_, 1);
  EXPECT_EQ(num_reported_, 2);
  EXPECT_EQ(num_correct_, 1);  // Still 1.
}

}  // namespace learning
}  // namespace media
