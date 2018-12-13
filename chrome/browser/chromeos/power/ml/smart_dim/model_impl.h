// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model.h"

namespace assist_ranker {
class ExamplePreprocessor;
class ExamplePreprocessorConfig;
}  // namespace assist_ranker

namespace chromeos {
namespace power {
namespace ml {

namespace tfnative_model {
struct FixedAllocations;
}  // namespace tfnative_model

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmartDimModelResult {
  kSuccess = 0,
  kPreprocessorInitializationFailed = 1,
  kPreprocessorOtherError = 2,
  kOtherError = 3,
  kMaxValue = kOtherError
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SmartDimParameterResult {
  kSuccess = 0,
  kUndefinedError = 1,
  kParsingError = 2,
  kMaxValue = kParsingError
};

// Real implementation of SmartDimModel that predicts whether an upcoming screen
// dim should go ahead based on user activity/inactivity following dim.
class SmartDimModelImpl : public SmartDimModel {
 public:
  SmartDimModelImpl();
  ~SmartDimModelImpl() override;

  // chromeos::power::ml::SmartDimModel overrides:
  void RequestDimDecision(const UserActivityEvent::Features& features,
                          DimDecisionCallback dim_callback) override;
  void CancelPreviousRequest() override;

 private:
  friend class SmartDimModelImplTest;
  // Loads the preprocessor config if not already loaded.
  void LazyInitialize();

  // Calculates inactivity score and returns result status. Also logs the status
  // to UMA.
  SmartDimModelResult CalculateInactivityScore(
      const UserActivityEvent::Features& features,
      float* inactivity_score_out);

  // Asynchronous task which gets posted to the |blocking_task_runner_| and
  // performs the ML inference call. Based on the result of the inference call,
  // a prediction is returned, and this return value will be passed to a
  // DimDecisionCallback which will execute after this function.
  UserActivityEvent::ModelPrediction ShouldDim(
      const UserActivityEvent::Features& input_features);

  std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
      preprocessor_config_;
  std::unique_ptr<assist_ranker::ExamplePreprocessor> preprocessor_;

  // Fixed-size working memory provided to the inferencing function. Lazily
  // initialized once so it isn't reallocated for every inference.
  std::unique_ptr<tfnative_model::FixedAllocations> model_alloc_;

  // A separate task runner is used to perform the inference.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::CancelableOnceCallback<void(UserActivityEvent::ModelPrediction)>
      cancelable_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SmartDimModelImpl);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
