// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BACKEND_DATA_TYPE_CONFIGURER_H_
#define CHROME_BROWSER_SYNC_GLUE_BACKEND_DATA_TYPE_CONFIGURER_H_
#pragma once

#include "base/callback.h"
#include "sync/internal_api/public/configure_reason.h"
#include "sync/internal_api/public/syncable/model_type.h"

namespace browser_sync {

// The DataTypeConfigurer interface abstracts out the action of
// configuring a set of new data types and cleaning up after a set of
// removed data types.
class BackendDataTypeConfigurer {
 public:
  // Enum representing the two "modes" of configuration.
  // WITHOUT_NIGORI is only used by migration.
  enum NigoriState { WITHOUT_NIGORI, WITH_NIGORI };

  // Configures the given set of types to add and remove.
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
      syncable::ModelTypeSet types_to_add,
      syncable::ModelTypeSet types_to_remove,
      NigoriState nigori_state,
      base::Callback<void(syncable::ModelTypeSet)> ready_task,
      base::Callback<void()> retry_callback) = 0;

 protected:
  virtual ~BackendDataTypeConfigurer() {}
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BACKEND_DATA_TYPE_CONFIGURER_H_
