// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "media/learning/common/learning_session.h"
#include "media/learning/mojo/mojo_learning_session_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class MojoLearningSessionImplTest : public ::testing::Test {
 public:
  class FakeLearningSession : public ::media::learning::LearningSession {
   public:
    void AddExample(const std::string& task_name,
                    const TrainingExample& example) override {
      most_recent_task_name_ = task_name;
      most_recent_example_ = example;
    }

    std::string most_recent_task_name_;
    TrainingExample most_recent_example_;
  };

 public:
  MojoLearningSessionImplTest() = default;
  ~MojoLearningSessionImplTest() override = default;

  void SetUp() override {
    // Create a mojo learner that forwards to a fake learner.
    std::unique_ptr<FakeLearningSession> provider =
        std::make_unique<FakeLearningSession>();
    fake_learning_session_ = provider.get();
    learning_session_impl_ = base::WrapUnique<MojoLearningSessionImpl>(
        new MojoLearningSessionImpl(std::move(provider)));
  }

  FakeLearningSession* fake_learning_session_ = nullptr;

  const mojom::LearningTaskType task_type_ =
      mojom::LearningTaskType::kPlaceHolderTask;

  // The learner provider under test.
  std::unique_ptr<MojoLearningSessionImpl> learning_session_impl_;
};

TEST_F(MojoLearningSessionImplTest, FeaturesAndTargetValueAreCopied) {
  mojom::TrainingExamplePtr example_ptr = mojom::TrainingExample::New();
  const TrainingExample example = {{Value(123), Value(456), Value(890)},
                                   TargetValue(1234)};

  learning_session_impl_->AddExample(task_type_, example);
  EXPECT_EQ(example, fake_learning_session_->most_recent_example_);
}

}  // namespace learning
}  // namespace media
