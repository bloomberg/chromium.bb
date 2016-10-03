// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_IMPL_MODEL_TYPE_CONNECTOR_PROXY_H_
#define COMPONENTS_SYNC_CORE_IMPL_MODEL_TYPE_CONNECTOR_PROXY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/core/model_type_connector.h"

namespace syncer_v2 {

// Proxies all ModelTypeConnector calls to another thread. Typically used by
// the SyncBackend to call from the UI thread to the real ModelTypeConnector on
// the sync thread.
class ModelTypeConnectorProxy : public ModelTypeConnector {
 public:
  ModelTypeConnectorProxy(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::WeakPtr<ModelTypeConnector>& model_type_connector);
  ~ModelTypeConnectorProxy() override;

  // ModelTypeConnector implementation
  void ConnectType(
      syncer::ModelType type,
      std::unique_ptr<ActivationContext> activation_context) override;
  void DisconnectType(syncer::ModelType type) override;

 private:
  // A SequencedTaskRunner representing the thread where the ModelTypeConnector
  // lives.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The ModelTypeConnector this object is wrapping.
  base::WeakPtr<ModelTypeConnector> model_type_connector_;
};

}  // namespace syncer_v2

#endif  // COMPONENTS_SYNC_CORE_IMPL_MODEL_TYPE_CONNECTOR_PROXY_H_
