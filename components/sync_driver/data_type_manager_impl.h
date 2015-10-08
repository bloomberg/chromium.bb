// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__

#include "components/sync_driver/data_type_manager.h"

#include <map>
#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync_driver/backend_data_type_configurer.h"
#include "components/sync_driver/model_association_manager.h"

namespace syncer {
struct DataTypeConfigurationStats;
class DataTypeDebugInfoListener;
template <typename T> class WeakHandle;
}

namespace sync_driver {

class DataTypeController;
class DataTypeEncryptionHandler;
class DataTypeManagerObserver;

// List of data types grouped by priority and ordered from high priority to
// low priority.
typedef std::queue<syncer::ModelTypeSet> TypeSetPriorityList;

class DataTypeManagerImpl : public DataTypeManager,
                            public ModelAssociationManagerDelegate {
 public:
  DataTypeManagerImpl(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      BackendDataTypeConfigurer* configurer,
      DataTypeManagerObserver* observer);
  ~DataTypeManagerImpl() override;

  // DataTypeManager interface.
  void Configure(syncer::ModelTypeSet desired_types,
                 syncer::ConfigureReason reason) override;
  void ReenableType(syncer::ModelType type) override;
  void ResetDataTypeErrors() override;

  // Needed only for backend migration.
  void PurgeForMigration(syncer::ModelTypeSet undesired_types,
                         syncer::ConfigureReason reason) override;

  void Stop() override;
  State state() const override;

  // |ModelAssociationManagerDelegate| implementation.
  void OnSingleDataTypeAssociationDone(
      syncer::ModelType type,
      const syncer::DataTypeAssociationStats& association_stats) override;
  void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) override;
  void OnSingleDataTypeWillStop(syncer::ModelType type,
                                const syncer::SyncError& error) override;

  // Used by unit tests. TODO(sync) : This would go away if we made
  // this class be able to do Dependency injection. crbug.com/129212.
  ModelAssociationManager* GetModelAssociationManagerForTesting() {
    return &model_association_manager_;
  }

 private:
  // Helper enum for identifying which types within a priority group to
  // associate.
  enum AssociationGroup {
    // Those types that were already downloaded and didn't have an error at
    // configuration time. Corresponds with AssociationTypesInfo's
    // |ready_types|. These types can start associating as soon as the
    // ModelAssociationManager is not busy.
    READY_AT_CONFIG,
    // All other types, including first time sync types and those that have
    // encountered an error. These types must wait until the syncer has done
    // any db changes and/or downloads before associating.
    UNREADY_AT_CONFIG,
  };

  friend class TestDataTypeManager;

  // Abort configuration and stop all data types due to configuration errors.
  void Abort(ConfigureStatus status);

  // Returns the priority types (control + priority user types).
  // Virtual for overriding during tests.
  virtual syncer::ModelTypeSet GetPriorityTypes() const;

  // Divide |types| into sets by their priorities and return the sets from
  // high priority to low priority.
  TypeSetPriorityList PrioritizeTypes(const syncer::ModelTypeSet& types);

  // Post a task to reconfigure when no downloading or association are running.
  void ProcessReconfigure();

  void Restart(syncer::ConfigureReason reason);
  void DownloadReady(syncer::ModelTypeSet types_to_download,
                     syncer::ModelTypeSet first_sync_types,
                     syncer::ModelTypeSet failed_configuration_types);

  // Notification from the SBH that download failed due to a transient
  // error and it will be retried.
  void OnDownloadRetry();
  void NotifyStart();
  void NotifyDone(const ConfigureResult& result);

  // Add to |configure_time_delta_| the time since we last called
  // Restart().
  void AddToConfigureTime();

  void ConfigureImpl(syncer::ModelTypeSet desired_types,
                     syncer::ConfigureReason reason);

  BackendDataTypeConfigurer::DataTypeConfigStateMap
  BuildDataTypeConfigStateMap(
      const syncer::ModelTypeSet& types_being_configured) const;

  // Start download of next set of types in |download_types_queue_| (if
  // any exist, does nothing otherwise).
  // Will kick off association of any new ready types.
  void StartNextDownload(syncer::ModelTypeSet high_priority_types_before);

  // Start association of next batch of data types after association of
  // previous batch finishes. |group| controls which set of types within
  // an AssociationTypesInfo to associate. Does nothing if model associator
  // is busy performing association.
  void StartNextAssociation(AssociationGroup group);

  void StopImpl();

  BackendDataTypeConfigurer* configurer_;
  // Map of all data type controllers that are available for sync.
  // This list is determined at startup by various command line flags.
  const DataTypeController::TypeMap* controllers_;
  State state_;
  std::map<syncer::ModelType, int> start_order_;
  syncer::ModelTypeSet last_requested_types_;

  // Whether an attempt to reconfigure was made while we were busy configuring.
  // The |last_requested_types_| will reflect the newest set of requested types.
  bool needs_reconfigure_;

  // The reason for the last reconfigure attempt. Note: this will be set to a
  // valid value only when |needs_reconfigure_| is set.
  syncer::ConfigureReason last_configure_reason_;

  // The last time Restart() was called.
  base::Time last_restart_time_;

  // The accumulated time spent between calls to Restart() and going
  // to the DONE state.
  base::TimeDelta configure_time_delta_;

  // Sync's datatype debug info listener, which we pass model association
  // statistics to.
  const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
      debug_info_listener_;

  // The manager that handles the model association of the individual types.
  ModelAssociationManager model_association_manager_;

  // DataTypeManager must have only one observer -- the ProfileSyncService that
  // created it and manages its lifetime.
  DataTypeManagerObserver* const observer_;

  // For querying failed data types (having unrecoverable error) when
  // configuring backend.
  DataTypeStatusTable data_type_status_table_;

  // Types waiting to be downloaded.
  TypeSetPriorityList download_types_queue_;

  // Types waiting for association and related time tracking info.
  struct AssociationTypesInfo {
    AssociationTypesInfo();
    ~AssociationTypesInfo();

    // Types to associate.
    syncer::ModelTypeSet types;
    // Types that have just been downloaded and are being associated for the
    // first time. This includes types that had previously encountered an error
    // and had to be purged/unapplied from the sync db.
    // This is a subset of |types|.
    syncer::ModelTypeSet first_sync_types;
    // Types that were already ready for association at configuration time.
    syncer::ModelTypeSet ready_types;
    // Time at which |types| began downloading.
    base::Time download_start_time;
    // Time at which |types| finished downloading.
    base::Time download_ready_time;
    // Time at which the association for |read_types| began.
    base::Time ready_association_request_time;
    // Time at which the association for |types| began (not relevant to
    // |ready_types|.
    base::Time full_association_request_time;
    // The set of types that are higher priority (and were therefore blocking)
    // the association of |types|.
    syncer::ModelTypeSet high_priority_types_before;
    // The subset of |types| that were successfully configured.
    syncer::ModelTypeSet configured_types;
  };
  std::queue<AssociationTypesInfo> association_types_queue_;

  // The encryption handler lets the DataTypeManager know the state of sync
  // datatype encryption.
  const DataTypeEncryptionHandler* encryption_handler_;

  // Association and time stats of data type configuration.
  std::vector<syncer::DataTypeConfigurationStats> configuration_stats_;

  // True iff we are in the process of catching up datatypes.
  bool catch_up_in_progress_;

  base::WeakPtrFactory<DataTypeManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeManagerImpl);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_MANAGER_IMPL_H__
