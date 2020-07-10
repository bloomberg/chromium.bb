// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_connector_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "components/sync/engine/data_type_activation_response.h"

namespace syncer {

ModelTypeConnectorProxy::ModelTypeConnectorProxy(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<ModelTypeConnector>& model_type_connector)
    : task_runner_(task_runner), model_type_connector_(model_type_connector) {}

ModelTypeConnectorProxy::~ModelTypeConnectorProxy() {}

void ModelTypeConnectorProxy::ConnectNonBlockingType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelTypeConnector::ConnectNonBlockingType,
                                model_type_connector_, type,
                                std::move(activation_response)));
}

void ModelTypeConnectorProxy::DisconnectNonBlockingType(ModelType type) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelTypeConnector::DisconnectNonBlockingType,
                                model_type_connector_, type));
}

void ModelTypeConnectorProxy::RegisterDirectoryType(ModelType type,
                                                    ModelSafeGroup group) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelTypeConnector::RegisterDirectoryType,
                                model_type_connector_, type, group));
}

void ModelTypeConnectorProxy::UnregisterDirectoryType(ModelType type) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelTypeConnector::UnregisterDirectoryType,
                                model_type_connector_, type));
}

}  // namespace syncer
