// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/learning/impl/learning_session_impl.h"
#include "media/learning/impl/learning_task_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningSessionImplTest : public testing::Test {
 public:
  class FakeLearningTaskController : public LearningTaskController {
   public:
    FakeLearningTaskController(const LearningTask& task,
                               SequenceBoundFeatureProvider feature_provider)
        : feature_provider_(std::move(feature_provider)) {
      // As a complete hack, call the only public method on fp so that
      // we can verify that it was given to us by the session.
      if (!feature_provider_.is_null()) {
        feature_provider_.Post(FROM_HERE, &FeatureProvider::AddFeatures,
                               FeatureVector(),
                               FeatureProvider::FeatureVectorCB());
      }
    }

    void AddExample(const LabelledExample& example) override {
      example_ = example;
    }

    SequenceBoundFeatureProvider feature_provider_;
    LabelledExample example_;
  };

  class FakeFeatureProvider : public FeatureProvider {
   public:
    FakeFeatureProvider(bool* flag_ptr) : flag_ptr_(flag_ptr) {}

    // Do nothing, except note that we were called.
    void AddFeatures(FeatureVector features,
                     FeatureProvider::FeatureVectorCB cb) override {
      *flag_ptr_ = true;
    }

    bool* flag_ptr_ = nullptr;
  };

  using ControllerVector = std::vector<FakeLearningTaskController*>;

  LearningSessionImplTest() {
    session_ = std::make_unique<LearningSessionImpl>();
    session_->SetTaskControllerFactoryCBForTesting(base::BindRepeating(
        [](ControllerVector* controllers, const LearningTask& task,
           SequenceBoundFeatureProvider feature_provider)
            -> std::unique_ptr<LearningTaskController> {
          auto controller = std::make_unique<FakeLearningTaskController>(
              task, std::move(feature_provider));
          controllers->push_back(controller.get());
          return controller;
        },
        &task_controllers_));

    task_0_.name = "task_0";
    task_1_.name = "task_1";
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

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

  LabelledExample example_0({FeatureValue(123), FeatureValue(456)},
                            TargetValue(1234));
  session_->AddExample(task_0_.name, example_0);

  LabelledExample example_1({FeatureValue(321), FeatureValue(654)},
                            TargetValue(4321));
  session_->AddExample(task_1_.name, example_1);
  EXPECT_EQ(task_controllers_[0]->example_, example_0);
  EXPECT_EQ(task_controllers_[1]->example_, example_1);
}

TEST_F(LearningSessionImplTest, FeatureProviderIsForwarded) {
  // Verify that a FeatureProvider actually gets forwarded to the LTC.
  bool flag = false;
  session_->RegisterTask(task_0_,
                         base::SequenceBound<FakeFeatureProvider>(
                             base::SequencedTaskRunnerHandle::Get(), &flag));
  scoped_task_environment_.RunUntilIdle();
  // Registering the task should create a FakeLearningTaskController, which will
  // call AddFeatures on the fake FeatureProvider.
  EXPECT_TRUE(flag);
}

}  // namespace learning
}  // namespace media
