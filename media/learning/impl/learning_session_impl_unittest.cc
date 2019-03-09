// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/impl/learning_session_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningSessionImplTest : public testing::Test {
 public:
  class FakeLearningTaskController;
  using ControllerVector = std::vector<FakeLearningTaskController*>;
  using TaskRunnerVector = std::vector<base::SequencedTaskRunner*>;

  class FakeLearningTaskController : public LearningTaskController {
   public:
    // Send ControllerVector* as void*, else it complains that args can't be
    // forwarded.  Adding base::Unretained() doesn't help.
    FakeLearningTaskController(void* controllers,
                               const LearningTask& task,
                               SequenceBoundFeatureProvider feature_provider)
        : feature_provider_(std::move(feature_provider)) {
      static_cast<ControllerVector*>(controllers)->push_back(this);
      // As a complete hack, call the only public method on fp so that
      // we can verify that it was given to us by the session.
      if (!feature_provider_.is_null()) {
        feature_provider_.Post(FROM_HERE, &FeatureProvider::AddFeatures,
                               FeatureVector(),
                               FeatureProvider::FeatureVectorCB());
      }
    }

    void BeginObservation(ObservationId id,
                          const FeatureVector& features) override {
      id_ = id;
      features_ = features;
    }

    void CompleteObservation(ObservationId id,
                             const ObservationCompletion& completion) override {
      EXPECT_EQ(id_, id);
      example_.features = std::move(features_);
      example_.target_value = completion.target_value;
      example_.weight = completion.weight;
    }

    void CancelObservation(ObservationId id) override { ASSERT_TRUE(false); }

    SequenceBoundFeatureProvider feature_provider_;
    ObservationId id_ = 0;
    FeatureVector features_;
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

  LearningSessionImplTest() {
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
    session_ = std::make_unique<LearningSessionImpl>(task_runner_);
    session_->SetTaskControllerFactoryCBForTesting(base::BindRepeating(
        [](ControllerVector* controllers, TaskRunnerVector* task_runners,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           const LearningTask& task,
           SequenceBoundFeatureProvider feature_provider)
            -> base::SequenceBound<LearningTaskController> {
          task_runners->push_back(task_runner.get());
          return base::SequenceBound<FakeLearningTaskController>(
              task_runner, static_cast<void*>(controllers), task,
              std::move(feature_provider));
        },
        &task_controllers_, &task_runners_));

    task_0_.name = "task_0";
    task_1_.name = "task_1";
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<LearningSessionImpl> session_;

  LearningTask task_0_;
  LearningTask task_1_;

  ControllerVector task_controllers_;
  TaskRunnerVector task_runners_;
};

TEST_F(LearningSessionImplTest, RegisteringTasksCreatesControllers) {
  EXPECT_EQ(task_controllers_.size(), 0u);
  EXPECT_EQ(task_runners_.size(), 0u);

  session_->RegisterTask(task_0_);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_.size(), 1u);
  EXPECT_EQ(task_runners_.size(), 1u);
  EXPECT_EQ(task_runners_[0], task_runner_.get());

  session_->RegisterTask(task_1_);
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_.size(), 2u);
  EXPECT_EQ(task_runners_.size(), 2u);
  EXPECT_EQ(task_runners_[1], task_runner_.get());
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

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->example_, example_0);
  EXPECT_EQ(task_controllers_[1]->example_, example_1);
}

TEST_F(LearningSessionImplTest, FeatureProviderIsForwarded) {
  // Verify that a FeatureProvider actually gets forwarded to the LTC.
  bool flag = false;
  session_->RegisterTask(
      task_0_, base::SequenceBound<FakeFeatureProvider>(task_runner_, &flag));
  scoped_task_environment_.RunUntilIdle();
  // Registering the task should create a FakeLearningTaskController, which will
  // call AddFeatures on the fake FeatureProvider.
  EXPECT_TRUE(flag);
}

}  // namespace learning
}  // namespace media
