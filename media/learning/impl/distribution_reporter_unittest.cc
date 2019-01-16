// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/distribution_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class DistributionReporterTest : public testing::Test {
 public:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  LearningTask task_;

  std::unique_ptr<DistributionReporter> reporter_;
};

TEST_F(DistributionReporterTest, DistributionReporterDoesNotCrash) {
  task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);

  const TargetValue Zero(0);
  const TargetValue One(1);

  TargetDistribution observed;
  // Observe an average of 2 / 3.
  observed[Zero] = 100;
  observed[One] = 200;
  auto cb = reporter_->GetPredictionCallback(observed);

  TargetDistribution predicted;
  // Predict an average of 5 / 9.
  predicted[Zero] = 40;
  predicted[One] = 50;
  std::move(cb).Run(predicted);

  // TODO(liberato): When we switch to ukm, use a TestUkmRecorder to make sure
  // that it fills in the right stuff.
  // https://chromium-review.googlesource.com/c/chromium/src/+/1385107 .
}

}  // namespace learning
}  // namespace media
