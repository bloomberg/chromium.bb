// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BACKEND_DATA_TYPE_CONFIGURER_H_
#define CHROME_BROWSER_SYNC_GLUE_BACKEND_DATA_TYPE_CONFIGURER_H_

#include <map>

#include "base/callback.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/configure_reason.h"

namespace browser_sync {

// The DataTypeConfigurer interface abstracts out the action of
// configuring a set of new data types and cleaning up after a set of
// removed data types.
class BackendDataTypeConfigurer {
 public:
  enum DataTypeConfigState {
    ENABLED,   // Actively being synced.
    DISABLED,  // Not syncing. Disabled by user.
    FAILED,    // Not syncing due to unrecoverable error.
  };
  typedef std::map<syncer::ModelType, DataTypeConfigState>
      DataTypeConfigStateMap;

  // Configures sync for data types in config_state_map according to the states.
  // |ready_task| is called on the same thread as ConfigureDataTypes
  // is called when configuration is done with the set of data types
  // that failed configuration (i.e., configuration succeeded iff that
  // set is empty).
  //
  // TODO(akalin): Use a Delegate class with
  // OnConfigureSuccess/OnConfigureFailure/OnConfigureRetry instead of
  // a pair of callbacks.  The awkward part is handling when
  // SyncBackendHost calls ConfigureDataTypes on itself to configure
  // Nigori.
  virtual void ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(syncer::ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) = 0;

  // Return model types in |state_map| that match |state|.
  static syncer::ModelTypeSet GetDataTypesInState(
      DataTypeConfigState state, const DataTypeConfigStateMap& state_map);

  // Set state of |types| in |state_map| to |state|.
  static void SetDataTypesState(DataTypeConfigState state,
                                syncer::ModelTypeSet types,
                                DataTypeConfigStateMap* state_map);

 protected:
  virtual ~BackendDataTypeConfigurer() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BACKEND_DATA_TYPE_CONFIGURER_H_
