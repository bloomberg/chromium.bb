// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_MANAGER_H_
#define COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "sync/internal_api/public/base/model_type.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {
class ModelTypeSyncProxyImpl;
class SyncContextProxy;
}  //namespace syncer

namespace sync_driver {

class NonBlockingDataTypeController;

// Manages a set of NonBlockingDataTypeControllers.
//
// Each NonBlockingDataTypeController instance handles the logic around
// enabling and disabling sync for a particular non-blocking type.  This class
// manages all the controllers.
class NonBlockingDataTypeManager {
 public:
  NonBlockingDataTypeManager();
  ~NonBlockingDataTypeManager();

  // Declares |type| as a non-blocking type.  Should be done early
  // on during sync initialization.
  //
  // The |preferred| flag indicates whether or not this type should be synced.
  void RegisterType(syncer::ModelType type, bool preferred);

  // Connects the ModelTypeSyncProxyImpl and associated model type
  // thread to its NonBlockingDataTypeController on the UI thread.
  void InitializeType(
      syncer::ModelType type,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::WeakPtr<syncer::ModelTypeSyncProxyImpl>& type_sync_proxy);

  // Connects the sync backend, as represented by a SyncContextProxy, to the
  // NonBlockingDataTypeController on the UI thread.
  void ConnectSyncBackend(scoped_ptr<syncer::SyncContextProxy> proxy);

  // Disconnects the sync backend from the UI thread.  Should be called
  // early on during shutdown, but the whole procedure is asynchronous so
  // there's not much downside to calling it later.
  void DisconnectSyncBackend();

  // Updates the set of types the user wants to have synced.
  void SetPreferredTypes(syncer::ModelTypeSet types);

  // Returns the list of all known non-blocking sync types that registered with
  // RegisterType.
  syncer::ModelTypeSet GetRegisteredTypes() const;

 private:
  typedef
      std::map<syncer::ModelType, NonBlockingDataTypeController*>
      NonBlockingDataTypeControllerMap;

  // List of data type controllers for non-blocking types.
  NonBlockingDataTypeControllerMap non_blocking_data_type_controllers_;

  // Deleter for elements of the non-blocking data types controller map.
  STLValueDeleter<NonBlockingDataTypeControllerMap>
      non_blocking_data_type_controllers_deleter_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_MANAGER_H_
