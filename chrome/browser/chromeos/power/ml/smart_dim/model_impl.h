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
#include "chrome/browser/chromeos/power/ml/smart_dim/ml_service_client.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model.h"

namespace assist_ranker {
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
  kMismatchedFeatureSizeError = 4,
  kMlServiceInitializationFailedError = 5,
  kMaxValue = kMlServiceInitializationFailedError
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
  // Loads the preprocessor config if not already loaded. Also initializes the
  // MlServiceClient object if the ML Service is being used for inference.
  void LazyInitialize();

  // Pre-processes the input features into a vector, placed in
  // |*vectorized_features|, which is consumable by the ML model.
  //
  // Returns SmartDimModelResult::kSuccess on success, and the appropriate
  // error on failure.
  SmartDimModelResult PreprocessInput(
      const UserActivityEvent::Features& features,
      std::vector<float>* vectorized_features);

  // Calculates inactivity score and returns result status. Also logs the status
  // to UMA.
  //
  // This codepath is used when the inference calls are made to TF-Native.
  SmartDimModelResult CalculateInactivityScoreTfNative(
      const UserActivityEvent::Features& features,
      float* inactivity_score_out);

  // Takes an |inactivity_score| returned from the ML model and, using that,
  // returns a ModelPrediction.
  //
  // NOTE: This function is only expected to be called after successful
  // inference executions, and therefore does not have an error path.
  // As a corollary, it also performs the logging of a successful inference
  // call to UMA (doing this here allows the logging to take place in a Mojo
  // callback in case the ML service was used).
  UserActivityEvent::ModelPrediction CreatePredictionFromInactivityScore(
      float inactivity_score);

  // Task which gets posted to the |blocking_task_runner_| and
  // performs the ML inference call. Based on the result of the inference call,
  // a prediction is returned, and this return value will be passed to a
  // DimDecisionCallback which will execute after this function.
  UserActivityEvent::ModelPrediction ShouldDimTfNative(
      const UserActivityEvent::Features& input_features);

  // Calls the ML service Mojo API to perform an Smart Dim inference call,
  // given |input_features|. The |callback| is supplied to the Mojo API,
  // which in turn will call it to provide the result (a ModelPrediction), once
  // the inference is complete.
  void ShouldDimMlService(const UserActivityEvent::Features& input_features,
                          DimDecisionCallback callback);

  std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
      preprocessor_config_;

  // Fixed-size working memory provided to the inferencing function. Lazily
  // initialized once so it isn't reallocated for every inference.
  std::unique_ptr<tfnative_model::FixedAllocations> model_alloc_;

  // A separate task runner is used to perform the TF Native inference.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Cancelable wrapper for the DimDecisionCallback passed by the client to
  // RequestDimDecision().
  base::CancelableOnceCallback<void(UserActivityEvent::ModelPrediction)>
      cancelable_callback_;

  const bool use_ml_service_;
  // Pointer to the object that handles the ML service calls. This pointer
  // is only initialized if features::kSmartDimMLService is set to true.
  std::unique_ptr<MlServiceClient> ml_service_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SmartDimModelImpl);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_MODEL_IMPL_H_
