// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/ml/smart_dim/ml_service_client.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/model_impl.h"
#include "mojo/public/cpp/bindings/map.h"

using ::chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using ::chromeos::machine_learning::mojom::ExecuteResult;
using ::chromeos::machine_learning::mojom::FloatList;
using ::chromeos::machine_learning::mojom::Int64List;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::ModelId;
using ::chromeos::machine_learning::mojom::ModelSpec;
using ::chromeos::machine_learning::mojom::ModelSpecPtr;
using ::chromeos::machine_learning::mojom::Tensor;
using ::chromeos::machine_learning::mojom::TensorPtr;
using ::chromeos::machine_learning::mojom::ValueList;

namespace chromeos {
namespace power {
namespace ml {

namespace {

// TODO(crbug.com/893425): This should exist in only one location, so it should
// be merged with its duplicate in model_impl.cc to a common location.
void LogPowerMLSmartDimModelResult(SmartDimModelResult result) {
  UMA_HISTOGRAM_ENUMERATION("PowerML.SmartDimModel.Result", result);
}

}  // namespace

MlServiceClient::MlServiceClient() : weak_factory_(this) {}

MlServiceClient::~MlServiceClient() {}

void MlServiceClient::LoadModelCallback(LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    // TODO(crbug.com/893425): Log to UMA.
    LOG(ERROR) << "Failed to load Smart Dim model.";
  }
}

void MlServiceClient::CreateGraphExecutorCallback(
    CreateGraphExecutorResult result) {
  if (result != CreateGraphExecutorResult::OK) {
    // TODO(crbug.com/893425): Log to UMA.
    LOG(ERROR) << "Failed to create Smart Dim Graph Executor.";
  }
}

void MlServiceClient::ExecuteCallback(
    base::Callback<UserActivityEvent::ModelPrediction(float)>
        get_prediction_callback,
    SmartDimModel::DimDecisionCallback decision_callback,
    const ExecuteResult result,
    const base::Optional<std::vector<TensorPtr>> outputs) {
  UserActivityEvent::ModelPrediction prediction;

  if (result != ExecuteResult::OK) {
    LOG(ERROR) << "Smart Dim inference execution failed.";
    prediction.set_response(UserActivityEvent::ModelPrediction::MODEL_ERROR);
    LogPowerMLSmartDimModelResult(SmartDimModelResult::kOtherError);
  } else {
    LogPowerMLSmartDimModelResult(SmartDimModelResult::kSuccess);
    float inactivity_score =
        (outputs.value())[0]->data->get_float_list()->value[0];
    prediction = get_prediction_callback.Run(inactivity_score);
  }

  std::move(decision_callback).Run(prediction);
}

void MlServiceClient::InitMlServiceHandlesIfNeeded() {
  if (!model_) {
    // Load the model.
    ModelSpecPtr spec = ModelSpec::New(ModelId::SMART_DIM);
    chromeos::machine_learning::ServiceConnection::GetInstance()->LoadModel(
        std::move(spec), mojo::MakeRequest(&model_),
        base::BindOnce(&MlServiceClient::LoadModelCallback,
                       weak_factory_.GetWeakPtr()));
  }

  if (!executor_) {
    // Get the graph executor.
    model_->CreateGraphExecutor(
        mojo::MakeRequest(&executor_),
        base::BindOnce(&MlServiceClient::CreateGraphExecutorCallback,
                       weak_factory_.GetWeakPtr()));
    executor_.set_connection_error_handler(base::BindOnce(
        &MlServiceClient::OnConnectionError, weak_factory_.GetWeakPtr()));
  }
}

void MlServiceClient::OnConnectionError() {
  // TODO(crbug.com/893425): Log to UMA.
  LOG(WARNING) << "Mojo connection for ML service closed.";
  executor_.reset();
  model_.reset();
}

void MlServiceClient::DoInference(
    const std::vector<float>& features,
    base::Callback<UserActivityEvent::ModelPrediction(float)>
        get_prediction_callback,
    SmartDimModel::DimDecisionCallback decision_callback) {
  InitMlServiceHandlesIfNeeded();

  // Prepare the input tensor.
  std::map<std::string, TensorPtr> inputs;
  auto tensor = Tensor::New();
  tensor->shape = Int64List::New();
  tensor->shape->value = std::vector<int64_t>({1, features.size()});
  tensor->data = ValueList::New();
  tensor->data->set_float_list(FloatList::New());
  tensor->data->get_float_list()->value =
      std::vector<double>(std::begin(features), std::end(features));
  inputs.emplace(std::string("input"), std::move(tensor));

  std::vector<std::string> outputs({std::string("output")});

  executor_->Execute(
      mojo::MapToFlatMap(std::move(inputs)), std::move(outputs),
      base::BindOnce(&MlServiceClient::ExecuteCallback,
                     weak_factory_.GetWeakPtr(), get_prediction_callback,
                     std::move(decision_callback)));
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
