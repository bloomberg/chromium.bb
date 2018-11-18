// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "media/learning/impl/learning_session_impl.h"
#include "media/learning/impl/learning_task_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningSessionImplTest : public testing::Test {
 public:
  class FakeLearningTaskController : public LearningTaskController {
   public:
    FakeLearningTaskController(const LearningTask& task) {}

    void AddExample(const TrainingExample& example) override {
      example_ = example;
    }

    TrainingExample example_;
  };

  using ControllerVector = std::vector<FakeLearningTaskController*>;

  LearningSessionImplTest() {
    session_ = std::make_unique<LearningSessionImpl>();
    session_->SetTaskControllerFactoryCBForTesting(base::BindRepeating(
        [](ControllerVector* controllers, const LearningTask& task)
            -> std::unique_ptr<LearningTaskController> {
          auto controller = std::make_unique<FakeLearningTaskController>(task);
          controllers->push_back(controller.get());
          return controller;
        },
        &task_controllers_));

    task_0_.name = "task_0";
    task_1_.name = "task_1";
  }

  std::unique_ptr<LearningSessionImpl> session_;

  LearningTask task_0_;
  LearningTask task_1_;

  ControllerVector task_controllers_;
};

TEST_F(LearningSessionImplTest, RegisteringTasksCreatesControllers) {
  EXPECT_EQ(task_controllers_.size(), 0u);
  session_->RegisterTask(task_0_);
  EXPECT_EQ(task_controllers_.size(), 1u);
  session_->RegisterTask(task_1_);
  EXPECT_EQ(task_controllers_.size(), 2u);
}

TEST_F(LearningSessionImplTest, ExamplesAreForwardedToCorrectTask) {
  session_->RegisterTask(task_0_);
  session_->RegisterTask(task_1_);

  TrainingExample example_0({FeatureValue(123), FeatureValue(456)},
                            TargetValue(1234));
  session_->AddExample(task_0_.name, example_0);

  TrainingExample example_1({FeatureValue(321), FeatureValue(654)},
                            TargetValue(4321));
  session_->AddExample(task_1_.name, example_1);
  EXPECT_EQ(task_controllers_[0]->example_, example_0);
  EXPECT_EQ(task_controllers_[1]->example_, example_1);
}

}  // namespace learning
}  // namespace media
