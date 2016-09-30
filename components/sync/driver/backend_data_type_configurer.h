// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_BACKEND_DATA_TYPE_CONFIGURER_H_
#define COMPONENTS_SYNC_DRIVER_BACKEND_DATA_TYPE_CONFIGURER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/core/configure_reason.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

class ChangeProcessor;
struct ActivationContext;

// The DataTypeConfigurer interface abstracts out the action of
// configuring a set of new data types and cleaning up after a set of
// removed data types.
class BackendDataTypeConfigurer {
 public:
  enum DataTypeConfigState {
    CONFIGURE_ACTIVE,    // Actively being configured. Data of such types
                         // will be downloaded if not present locally.
    CONFIGURE_INACTIVE,  // Already configured or to be configured in future.
                         // Data of such types is left as it is, no
                         // downloading or purging.
    CONFIGURE_CLEAN,     // Actively being configured but requiring unapply
                         // and GetUpdates first (e.g. for persistence errors).
    DISABLED,            // Not syncing. Disabled by user.
    FATAL,               // Not syncing due to unrecoverable error.
    CRYPTO,              // Not syncing due to a cryptographer error.
    UNREADY,             // Not syncing due to transient error.
  };
  typedef std::map<ModelType, DataTypeConfigState> DataTypeConfigStateMap;

  // Configures sync for data types in config_state_map according to the states.
  // |ready_task| is called on the same thread as ConfigureDataTypes
  // is called when configuration is done with the set of data types
  // that succeeded/failed configuration (i.e., configuration succeeded iff
  // the failed set is empty).
  // Returns: the set of types that are already configured and are ready to
  // start.
  //
  // TODO(akalin): Use a Delegate class with
  // OnConfigureSuccess/OnConfigureFailure/OnConfigureRetry instead of
  // a pair of callbacks.  The awkward part is handling when
  // SyncBackendHost calls ConfigureDataTypes on itself to configure
  // Nigori.
  virtual ModelTypeSet ConfigureDataTypes(
      ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) = 0;

  // Return model types in |state_map| that match |state|.
  static ModelTypeSet GetDataTypesInState(
      DataTypeConfigState state,
      const DataTypeConfigStateMap& state_map);

  // Activates change processing for the given directory data type.  This must
  // be called synchronously with the data type's model association so
  // no changes are dropped between model association and change
  // processor activation.
  virtual void ActivateDirectoryDataType(ModelType type,
                                         ModelSafeGroup group,
                                         ChangeProcessor* change_processor) = 0;

  // Deactivates change processing for the given data type.
  virtual void DeactivateDirectoryDataType(ModelType type) = 0;

  // Activates change processing for the given non-blocking data type.
  // This must be called before initial sync for data type.
  virtual void ActivateNonBlockingDataType(
      ModelType type,
      std::unique_ptr<ActivationContext> activation_context) = 0;

  // Deactivates change processing for the given non-blocking data type.
  virtual void DeactivateNonBlockingDataType(ModelType type) = 0;

  // Set state of |types| in |state_map| to |state|.
  static void SetDataTypesState(DataTypeConfigState state,
                                ModelTypeSet types,
                                DataTypeConfigStateMap* state_map);

 protected:
  virtual ~BackendDataTypeConfigurer() {}
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_BACKEND_DATA_TYPE_CONFIGURER_H_
