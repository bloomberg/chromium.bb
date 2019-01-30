// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_SERVICE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_SERVICE_CLIENT_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/tensor.mojom.h"

namespace chromeos {
namespace power {
namespace ml {

// ML service Mojo client which loads a Smart Dim model, and then performs
// inference on inputs provided by the caller.
class MlServiceClient {
 public:
  MlServiceClient();
  ~MlServiceClient();

  // Sends an input vector to the ML service to run inference on. It also
  // provides a |decision_callback| to the Mojo service which will be run
  // by the ExecuteCallback() on a return from the inference call.
  //
  // |get_prediction_callback| takes an inactivity score value returned
  // by the ML model and uses it to return a ModelPrediction which is fed
  // into the |decision_callback|.
  //
  // NOTE: A successful Mojo call *does not* guarantee a successful inference
  // call. The ExecuteCallback can be run with failure result, in case the
  // inference call failed.
  void DoInference(const std::vector<float>& features,
                   base::Callback<UserActivityEvent::ModelPrediction(float)>
                       get_prediction_callback,
                   SmartDimModel::DimDecisionCallback decision_callback);

 private:
  // Various callbacks that get invoked by the Mojo framework.
  void LoadModelCallback(
      ::chromeos::machine_learning::mojom::LoadModelResult result);
  void CreateGraphExecutorCallback(
      ::chromeos::machine_learning::mojom::CreateGraphExecutorResult result);

  // Callback executed by ML Service when an Execute call is complete.
  //
  // The |get_prediction_callback| and the |decision_callback| are bound
  // to the ExecuteCallback during while calling the Execute() function
  // on the Mojo API.
  void ExecuteCallback(
      base::Callback<UserActivityEvent::ModelPrediction(float)>
          get_prediction_callback,
      SmartDimModel::DimDecisionCallback decision_callback,
      ::chromeos::machine_learning::mojom::ExecuteResult result,
      base::Optional<
          std::vector<::chromeos::machine_learning::mojom::TensorPtr>> outputs);
  // Initializes the various handles to the ML service if they're not already
  // available.
  void InitMlServiceHandlesIfNeeded();

  void OnConnectionError();

  // Pointers used to execute functions in the ML service server end.
  ::chromeos::machine_learning::mojom::ModelPtr model_;
  ::chromeos::machine_learning::mojom::GraphExecutorPtr executor_;

  base::WeakPtrFactory<MlServiceClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MlServiceClient);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_SERVICE_CLIENT_H_
