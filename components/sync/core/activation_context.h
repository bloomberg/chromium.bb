// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_ACTIVATION_CONTEXT_H_
#define COMPONENTS_SYNC_CORE_ACTIVATION_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/base/sync_export.h"
#include "components/sync/core/model_type_processor.h"
#include "components/sync/core/non_blocking_sync_common.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer_v2 {

// The state passed from ModelTypeProcessor to Sync thread during DataType
// activation.
struct SYNC_EXPORT ActivationContext {
  ActivationContext();
  ~ActivationContext();

  // Initial DataTypeState at the moment of activation.
  sync_pb::DataTypeState data_type_state;

  // The ModelTypeProcessor for the worker. Note that this is owned because
  // it is generally a proxy object to the real processor.
  std::unique_ptr<ModelTypeProcessor> type_processor;
};

}  // namespace syncer_v2

#endif  // COMPONENTS_SYNC_CORE_ACTIVATION_CONTEXT_H_
