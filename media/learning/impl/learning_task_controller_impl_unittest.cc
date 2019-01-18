// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_task_controller_impl.h"

#include "base/bind.h"
#include "media/learning/impl/distribution_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest : public testing::Test {
 public:
  class FakeDistributionReporter : public DistributionReporter {
   public:
    FakeDistributionReporter(const LearningTask& task)
        : DistributionReporter(task) {}

   protected:
    void OnPrediction(TargetDistribution observed,
                      TargetDistribution predicted) override {
      num_reported_++;
      if (observed == predicted)
        num_correct_++;
    }

   public:
    int num_reported_ = 0;
    int num_correct_ = 0;
  };

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

  class FakeTrainer : public TrainingAlgorithm {
   public:
    // |num_models| is where we'll record how many models we've trained.
    // |target_value| is the prediction that our trained model will make.
    FakeTrainer(int* num_models, TargetValue target_value)
        : num_models_(num_models), target_value_(target_value) {}
    ~FakeTrainer() override {}

    void Train(const LearningTask& task,
               const TrainingData& training_data,
               TrainedModelCB model_cb) override {
      (*num_models_)++;
      std::move(model_cb).Run(std::make_unique<FakeModel>(target_value_));
    }

   private:
    int* num_models_ = nullptr;
    TargetValue target_value_;
  };

  LearningTaskControllerImplTest()
      : predicted_target_(123), not_predicted_target_(456) {
    // Don't require too many training examples per report.
    task_.max_data_set_size = 20;
    task_.min_new_data_fraction = 0.1;

    std::unique_ptr<FakeDistributionReporter> reporter =
        std::make_unique<FakeDistributionReporter>(task_);
    reporter_raw_ = reporter.get();

    controller_ = std::make_unique<LearningTaskControllerImpl>(
        task_, std::move(reporter));
    controller_->SetTrainerForTesting(
        std::make_unique<FakeTrainer>(&num_models_, predicted_target_));
  }

  // Number of models that we trained.
  int num_models_ = 0;

  // Two distinct targets.
  const TargetValue predicted_target_;
  const TargetValue not_predicted_target_;

  FakeDistributionReporter* reporter_raw_ = nullptr;

  LearningTask task_;
  std::unique_ptr<LearningTaskControllerImpl> controller_;
};

TEST_F(LearningTaskControllerImplTest, AddingExamplesTrainsModelAndReports) {
  LabelledExample example;

  // Up to the first 1/training_fraction examples should train on each example.
  // Make each of the examples agree on |predicted_target_|.
  example.target_value = predicted_target_;
  int count = static_cast<int>(1.0 / task_.min_new_data_fraction);
  for (int i = 0; i < count; i++) {
    controller_->AddExample(example);
    EXPECT_EQ(num_models_, i + 1);
    // All examples except the first should be reported as correct.  For the
    // first, there's no model to test again.
    EXPECT_EQ(reporter_raw_->num_reported_, i);
    EXPECT_EQ(reporter_raw_->num_correct_, i);
  }
  // The next |count| should train every other one.
  for (int i = 0; i < count; i++) {
    controller_->AddExample(example);
    EXPECT_EQ(num_models_, count + (i + 1) / 2);
  }

  // The next |count| should be the same, since we've reached the max training
  // set size.
  for (int i = 0; i < count; i++) {
    controller_->AddExample(example);
    EXPECT_EQ(num_models_, count + count / 2 + (i + 1) / 2);
  }

  // We should have reported results for each except the first.  All of them
  // should be correct, since there's only one target so far.
  EXPECT_EQ(reporter_raw_->num_reported_, count * 3 - 1);
  EXPECT_EQ(reporter_raw_->num_correct_, count * 3 - 1);

  // Adding a value that doesn't match should report one more attempt, with an
  // incorrect prediction.
  example.target_value = not_predicted_target_;
  controller_->AddExample(example);
  EXPECT_EQ(reporter_raw_->num_reported_, count * 3);
  EXPECT_EQ(reporter_raw_->num_correct_, count * 3 - 1);  // Unchanged.
}

}  // namespace learning
}  // namespace media
