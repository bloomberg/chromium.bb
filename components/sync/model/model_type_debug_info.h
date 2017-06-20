// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_DEBUG_INFO_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_DEBUG_INFO_H_

#include <memory>

#include "base/callback.h"
#include "base/values.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model_impl/shared_model_type_processor.h"

namespace syncer {

// This class holds static functions used for extracting debug information for a
// model type. They should be run on the model thread. These functions are
// static so they can be posted to from the UI thread, and they live inside a
// class because it is a friend class to SharedModelTypeProcessor.
class ModelTypeDebugInfo {
 public:
  // Returns a ListValue representing all nodes for the type to |callback|.
  // Used for populating nodes in Sync Node Browser of chrome://sync-internals.
  static void GetAllNodes(
      const base::Callback<void(const ModelType,
                                std::unique_ptr<base::ListValue>)>& callback,
      ModelTypeSyncBridge* bridge);

  // Returns StatusCounters for the type to |callback|.
  // Used for updating data type counters in chrome://sync-internals.
  static void GetStatusCounters(
      const base::Callback<void(const ModelType, const StatusCounters&)>&
          callback,
      ModelTypeSyncBridge* bridge);

  // Queries |bridge| for estimate of memory usage and records it in a
  // histogram.
  static void RecordMemoryUsageHistogram(ModelTypeSyncBridge* bridge);

 private:
  ModelTypeDebugInfo();

  // This is callback function for ModelTypeSyncBridge::GetAllData. This
  // function will merge real data from |batch| with metadata extracted from
  // the |processor|, then pass it all to |callback|.
  static void MergeDataWithMetadata(
      SharedModelTypeProcessor* processor,
      const base::Callback<void(const ModelType,
                                std::unique_ptr<base::ListValue>)>& callback,
      std::unique_ptr<DataBatch> batch);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_DEBUG_INFO_H_
