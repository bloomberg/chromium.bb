// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/distribution_reporter.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"

namespace media {
namespace learning {

// Low order bit is "observed", second bit is "predicted".
enum class ConfusionMatrix {
  TrueNegative = 0,   // predicted == observed == false
  FalseNegative = 1,  // predicted == false, observed == true
  FalsePositive = 2,  // predicted == true, observed == false
  TruePositive = 3,   // predicted == observed == true
  kMaxValue = TruePositive
};

// TODO(liberato): Currently, this implementation is a hack to collect some
// sanity-checking data for local learning with MediaCapabilities.  We assume
// that the prediction is the "percentage of dropped frames".
//
// Please see https://chromium-review.googlesource.com/c/chromium/src/+/1385107
// for an actual UKM-based implementation.
class RegressionReporter : public DistributionReporter {
 public:
  RegressionReporter(const LearningTask& task) : DistributionReporter(task) {}

  void OnPrediction(TargetDistribution observed,
                    TargetDistribution predicted) override {
    DCHECK_EQ(task().target_description.ordering,
              LearningTask::Ordering::kNumeric);
    DCHECK(!task().uma_hacky_confusion_matrix.empty());

    // As a complete hack, record accuracy with a fixed threshold.  The average
    // is the observed / predicted percentage of dropped frames.
    bool observed_smooth = observed.Average() <= task().smoothness_threshold;
    bool predicted_smooth = predicted.Average() <= task().smoothness_threshold;
    DVLOG(2) << "Learning: " << task().name
             << ": predicted: " << predicted_smooth << " ("
             << predicted.Average() << ") observed: " << observed_smooth << " ("
             << observed.Average() << ")";

    // Convert to a bucket from which we can get the confusion matrix.
    ConfusionMatrix uma_bucket = static_cast<ConfusionMatrix>(
        (observed_smooth ? 1 : 0) | (predicted_smooth ? 2 : 0));
    base::UmaHistogramEnumeration(task().uma_hacky_confusion_matrix,
                                  uma_bucket);
  }
};

std::unique_ptr<DistributionReporter> DistributionReporter::Create(
    const LearningTask& task) {
  // Hacky reporting is the only thing we know how to report.
  if (task.uma_hacky_confusion_matrix.empty())
    return nullptr;

  if (task.target_description.ordering == LearningTask::Ordering::kNumeric)
    return std::make_unique<RegressionReporter>(task);
  return nullptr;
}

DistributionReporter::DistributionReporter(const LearningTask& task)
    : task_(task), weak_factory_(this) {}

DistributionReporter::~DistributionReporter() = default;

Model::PredictionCB DistributionReporter::GetPredictionCallback(
    TargetDistribution observed) {
  return base::BindOnce(&DistributionReporter::OnPrediction,
                        weak_factory_.GetWeakPtr(), observed);
}

}  // namespace learning
}  // namespace media
