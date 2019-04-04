// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/distribution_reporter.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"

namespace media {
namespace learning {

// Low order bit is "observed", second bit is "predicted", third bit is "could
// not make a prediction".
enum class ConfusionMatrix {
  TrueNegative = 0,     // predicted == observed == false
  FalseNegative = 1,    // predicted == false, observed == true
  FalsePositive = 2,    // predicted == true, observed == false
  TruePositive = 3,     // predicted == observed == true
  SkippedNegative = 4,  // predicted == N/A, observed == false
  SkippedPositive = 5,  // predicted == N/A, observed == true
  kMaxValue = SkippedPositive
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

  void OnPrediction(TargetHistogram observed,
                    TargetHistogram predicted) override {
    DCHECK_EQ(task().target_description.ordering,
              LearningTask::Ordering::kNumeric);
    DCHECK(!task().uma_hacky_confusion_matrix.empty());

    // As a complete hack, record accuracy with a fixed threshold.  The average
    // is the observed / predicted percentage of dropped frames.
    bool observed_smooth = observed.Average() <= task().smoothness_threshold;

    // See if we made a prediction.
    int predicted_bits = 4;  // N/A
    if (predicted.total_counts() != 0) {
      bool predicted_smooth =
          predicted.Average() <= task().smoothness_threshold;
      DVLOG(2) << "Learning: " << task().name
               << ": predicted: " << predicted_smooth << " ("
               << predicted.Average() << ") observed: " << observed_smooth
               << " (" << observed.Average() << ")";
      predicted_bits = predicted_smooth ? 2 : 0;
    } else {
      DVLOG(2) << "Learning: " << task().name
               << ": predicted: N/A observed: " << observed_smooth << " ("
               << observed.Average() << ")";
    }

    // Convert to a bucket from which we can get the confusion matrix.
    ConfusionMatrix uma_bucket = static_cast<ConfusionMatrix>(
        (observed_smooth ? 1 : 0) | predicted_bits);

    if (!feature_indices() ||
        feature_indices()->size() == task().feature_descriptions.size()) {
      base::UmaHistogramEnumeration(task().uma_hacky_confusion_matrix,
                                    uma_bucket);
    } else if (feature_indices()->size() == 1) {
      // Slide the matrix over by whatever feature we're measuring.
      constexpr int matrix_size =
          static_cast<int>(ConfusionMatrix::kMaxValue) + 1;
      int value_max = matrix_size * task().feature_descriptions.size() - 1;
      int feature_index = *feature_indices()->begin();
      base::UmaHistogramExactLinear(
          task().uma_hacky_confusion_matrix,
          static_cast<int>(uma_bucket) + feature_index * matrix_size,
          value_max);
    }  // else do nothing -- we only support one feature for UMA reporting.
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
    TargetHistogram observed) {
  return base::BindOnce(&DistributionReporter::OnPrediction,
                        weak_factory_.GetWeakPtr(), observed);
}

void DistributionReporter::SetFeatureSubset(
    const std::set<int>& feature_indices) {
  feature_indices_ = feature_indices;
}

}  // namespace learning
}  // namespace media
