// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_connector_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/engine/activation_context.h"

namespace syncer {

ModelTypeConnectorProxy::ModelTypeConnectorProxy(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<ModelTypeConnector>& model_type_connector)
    : task_runner_(task_runner), model_type_connector_(model_type_connector) {}

ModelTypeConnectorProxy::~ModelTypeConnectorProxy() {}

void ModelTypeConnectorProxy::ConnectType(
    ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ModelTypeConnector::ConnectType, model_type_connector_, type,
                 base::Passed(&activation_context)));
}

void ModelTypeConnectorProxy::DisconnectType(ModelType type) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&ModelTypeConnector::DisconnectType,
                                    model_type_connector_, type));
}

}  // namespace syncer
