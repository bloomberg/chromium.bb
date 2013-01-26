// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__

#include "chrome/browser/sync/glue/data_type_manager.h"

#include <list>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/sync/glue/backend_data_type_configurer.h"
#include "chrome/browser/sync/glue/model_association_manager.h"

class FailedDatatypesHandler;

namespace syncer {
class DataTypeDebugInfoListener;
template <typename T> class WeakHandle;
}

namespace browser_sync {

class DataTypeController;
class DataTypeManagerObserver;

class DataTypeManagerImpl : public DataTypeManager,
                            public ModelAssociationResultProcessor {
 public:
  DataTypeManagerImpl(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      BackendDataTypeConfigurer* configurer,
      const DataTypeController::TypeMap* controllers,
      DataTypeManagerObserver* observer,
      const FailedDatatypesHandler* failed_datatypes_handler);
  virtual ~DataTypeManagerImpl();

  // DataTypeManager interface.
  virtual void Configure(TypeSet desired_types,
                         syncer::ConfigureReason reason) OVERRIDE;

  // Needed only for backend migration.
  virtual void PurgeForMigration(
      TypeSet undesired_types,
      syncer::ConfigureReason reason) OVERRIDE;

  virtual void Stop() OVERRIDE;
  virtual State state() const OVERRIDE;

  // |ModelAssociationResultProcessor| implementation.
  virtual void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) OVERRIDE;
  virtual void OnTypesLoaded() OVERRIDE;

  // Used by unit tests. TODO(sync) : This would go away if we made
  // this class be able to do Dependency injection. crbug.com/129212.
  ModelAssociationManager* GetModelAssociationManagerForTesting() {
    return &model_association_manager_;
  }

 private:
  // Stops all data types.
  void FinishStop();
  void Abort(ConfigureStatus status,
             const syncer::SyncError& error);

  // If there's a pending reconfigure, processes it and returns true.
  // Otherwise, returns false.
  bool ProcessReconfigure();

  void Restart(syncer::ConfigureReason reason);
  void DownloadReady(syncer::ModelTypeSet failed_configuration_types);

  // Notification from the SBH that download failed due to a transient
  // error and it will be retried.
  void OnDownloadRetry();
  void NotifyStart();
  void NotifyDone(const ConfigureResult& result);
  void SetBlockedAndNotify();

  // Add to |configure_time_delta_| the time since we last called
  // Restart().
  void AddToConfigureTime();

  void ConfigureImpl(TypeSet desired_types, syncer::ConfigureReason reason);

  BackendDataTypeConfigurer::DataTypeConfigStateMap
      BuildDataTypeConfigStateMap() const;

  BackendDataTypeConfigurer* configurer_;
  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const DataTypeController::TypeMap* controllers_;
  State state_;
  std::map<syncer::ModelType, int> start_order_;
  TypeSet last_requested_types_;

  // Whether an attempt to reconfigure was made while we were busy configuring.
  // The |last_requested_types_| will reflect the newest set of requested types.
  bool needs_reconfigure_;

  // The reason for the last reconfigure attempt. Note: this will be set to a
  // valid value only when |needs_reconfigure_| is set.
  syncer::ConfigureReason last_configure_reason_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // The accumulated time spent between calls to Restart() and going
  // to the DONE/BLOCKED state.
  base::TimeDelta configure_time_delta_;

  // Collects the list of errors resulting from failing to start a type. This
  // would eventually be sent to the listeners after all the types have
  // been given a chance to start.
  std::list<syncer::SyncError> failed_datatypes_info_;
  ModelAssociationManager model_association_manager_;

  // DataTypeManager must have only one observer -- the ProfileSyncService that
  // created it and manages its lifetime.
  DataTypeManagerObserver* const observer_;

  // For querying failed data types (having unrecoverable error) when
  // configuring backend.
  const FailedDatatypesHandler* failed_datatypes_handler_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeManagerImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_IMPL_H__
