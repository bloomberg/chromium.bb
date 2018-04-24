// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"

namespace syncer {

class DataBatch;

// The ModelTypeControllerDelegate handles communication of ModelTypeController
// with the data type.
class ModelTypeControllerDelegate {
 public:
  using DataCallback = base::OnceCallback<void(std::unique_ptr<DataBatch>)>;

  virtual ~ModelTypeControllerDelegate() = default;

  // Gathers additional information needed before the processor can be
  // connected to a sync worker. Once the metadata has been loaded, the info
  // is collected and given to |callback|.
  virtual void OnSyncStarting(
      const ModelErrorHandler& error_handler,
      ModelTypeChangeProcessor::StartCallback callback) = 0;

  // Indicates that we no longer want to do any sync-related things for this
  // data type. Severs all ties to the sync thread, deletes all local sync
  // metadata, and then destroys the change processor.
  virtual void DisableSync() = 0;

  // TODO(jkrcal): Move to following functions back to ModelTypeSyncBridge once
  // the Delegate gets implemented directly by the ChangeProcessor and thus we
  // can reveal the functions needed by ModelTypeDebugInfo directly on this
  // interface.

  // Asynchronously retrieve all of the local sync data. |callback| should be
  // invoked if the operation is successful, otherwise the processor's
  // ReportError method should be called.
  virtual void GetAllData(DataCallback callback) = 0;

  virtual ModelTypeChangeProcessor* change_processor() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_CONTROLLER_DELEGATE_H_
