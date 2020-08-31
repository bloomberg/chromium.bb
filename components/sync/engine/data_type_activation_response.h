// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_RESPONSE_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_RESPONSE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

// The state passed from ModelTypeProcessor to Sync thread during DataType
// activation.
struct DataTypeActivationResponse {
  DataTypeActivationResponse();
  ~DataTypeActivationResponse();

  // Initial ModelTypeState at the moment of activation.
  sync_pb::ModelTypeState model_type_state;

  // The ModelTypeProcessor for the worker. Note that this is owned because
  // it is generally a proxy object to the real processor.
  std::unique_ptr<ModelTypeProcessor> type_processor;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_RESPONSE_H_
