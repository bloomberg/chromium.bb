// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

#include "chrome/browser/sync/glue/data_type_manager.h"
#include "sync/internal_api/public/data_type_association_stats.h"
#include "sync/internal_api/public/util/weak_handle.h"

// |ModelAssociationManager| does the heavy lifting for doing the actual model
// association. It instructs DataTypeControllers to load models, start
// associating and stopping. Since the operations are async it uses an
// interface to inform DataTypeManager the results of the operations.
// This class is owned by DataTypeManager.
namespace browser_sync {

class DataTypeController;

// |ModelAssociationManager| association functions are async. The results of
// those operations are passed back via this interface.
class ModelAssociationResultProcessor {
 public:
  virtual void OnSingleDataTypeAssociationDone(
      syncer::ModelType type,
      const syncer::DataTypeAssociationStats& association_stats) = 0;
  virtual void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) = 0;
  virtual ~ModelAssociationResultProcessor() {}
};

// The class that is responsible for model association.
class ModelAssociationManager {
 public:
  ModelAssociationManager(const DataTypeController::TypeMap* controllers,
                          ModelAssociationResultProcessor* processor);
  virtual ~ModelAssociationManager();

  // Initializes the state to do the model association in future. This
  // should be called before communicating with sync server. A subsequent call
  // of Initialize is only allowed if the ModelAssociationManager has invoked
  // |OnModelAssociationDone| on the |ModelAssociationResultProcessor|. After
  // this call, there should be several calls to StartAssociationAsync()
  // to associate subset of |desired_types|.
  void Initialize(syncer::ModelTypeSet desired_types);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop();

  // Should only be called after Initialize to start the actual association.
  // |types_to_associate| should be subset of |desired_types| in Initialize().
  // When this is completed, |OnModelAssociationDone| will be invoked.
  void StartAssociationAsync(const syncer::ModelTypeSet& types_to_associate);

  // This is used for TESTING PURPOSE ONLY. The test case can inspect
  // and modify the timer.
  // TODO(sync) : This would go away if we made this class be able to do
  // Dependency injection. crbug.com/129212.
   base::OneShotTimer<ModelAssociationManager>* GetTimerForTesting();

 private:
  enum State {
    // This is the state after |Initialize| is called.
    INITIALIZED_TO_CONFIGURE,
    // Starting a new configuration.
    CONFIGURING,
    // No configuration is in progress.
    IDLE
  };

  // Called at the end of association to reset state to prepare for next
  // round of association.
  void ResetForNextAssociation();

  // Called by Initialize() to stop types that are not in |desired_types_|.
  void StopDisabledTypes();

  // Start loading non-running types that are in |desired_types_|.
  void LoadEnabledTypes();

  // Callback passed to each data type controller on starting association. This
  // callback will be invoked when the model association is done.
  void TypeStartCallback(syncer::ModelType type,
                         base::TimeTicks type_start_time,
                         DataTypeController::StartResult start_result,
                         const syncer::SyncMergeResult& local_merge_result,
                         const syncer::SyncMergeResult& syncer_merge_result);

  // Callback that will be invoked when the models finish loading. This callback
  // will be passed to |LoadModels| function.
  void ModelLoadCallback(syncer::ModelType type, syncer::SyncError error);

  // When a type fails to load or fails associating this method is invoked to
  // do the book keeping and do the UMA reporting.
  void AppendToFailedDatatypesAndLogError(const syncer::SyncError& error);

  // Called when all requested types are associated or association times out.
  // Notify |result_processor_| of configuration results.
  void ModelAssociationDone();

  State state_;

  // Data types that are enabled.
  syncer::ModelTypeSet desired_types_;

  // Data types that are requested to associate.
  syncer::ModelTypeSet requested_types_;

  // Data types currently being associated, including types waiting for model
  // load.
  syncer::ModelTypeSet associating_types_;

  // Data types that are loaded, i.e. ready to associate.
  syncer::ModelTypeSet loaded_types_;

  // Data types that are associated, i.e. no more action needed during
  // reconfiguration if not disabled.
  syncer::ModelTypeSet associated_types_;

  // Data types that are still loading/associating when configuration times
  // out.
  syncer::ModelTypeSet slow_types_;

  // Collects the list of errors resulting from failing to start a type. This
  // would eventually be sent to the listeners after all the types have
  // been given a chance to start.
  std::map<syncer::ModelType, syncer::SyncError> failed_data_types_info_;

  // The set of types that can't configure due to cryptographer errors.
  syncer::ModelTypeSet needs_crypto_types_;

  // Time when StartAssociationAsync() is called to associate for a set of data
  // types.
  base::TimeTicks association_start_time_;

  // Set of all registered controllers.
  const DataTypeController::TypeMap* controllers_;

  // The processor in charge of handling model association results.
  ModelAssociationResultProcessor* result_processor_;

  // Timer to track and limit how long a datatype takes to model associate.
  base::OneShotTimer<ModelAssociationManager> timer_;

  base::WeakPtrFactory<ModelAssociationManager> weak_ptr_factory_;

  DataTypeManager::ConfigureStatus configure_status_;

  DISALLOW_COPY_AND_ASSIGN(ModelAssociationManager);
};
}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
