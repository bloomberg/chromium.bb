// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__

#include <list>

#include "base/memory/weak_ptr.h"
#include "base/timer.h"

#include "chrome/browser/sync/glue/data_type_manager.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"
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
  virtual void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) = 0;
  virtual ~ModelAssociationResultProcessor() {}

  // Called to let the |ModelAssociationResultProcessor| know that "delayed"
  // types have finished loading and association should take place. (A delayed
  // type here is a type that did not finish loading during the previous
  // configure cycle.)
  virtual void OnTypesLoaded() = 0;
};

// The class that is responsible for model association.
class ModelAssociationManager {
 public:
  ModelAssociationManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const DataTypeController::TypeMap* controllers,
      ModelAssociationResultProcessor* processor);
  virtual ~ModelAssociationManager();

  // Initializes the state to do the model association in future. This
  // should be called before communicating with sync server. A subsequent call
  // of Initialize is only allowed if the ModelAssociationManager has invoked
  // |OnModelAssociationDone| on the |ModelAssociationResultProcessor|.
  void Initialize(syncer::ModelTypeSet desired_types);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop();

  // Should only be called after Initialize.
  // Starts the actual association. When this is completed
  // |OnModelAssociationDone| would be called invoked.
  void StartAssociationAsync();

  // It is valid to call this only when we are initialized to configure
  // but we have not started the configuration.(i.e., |Initialize| has
  // been called but |StartAssociationAsync| has not yet been called.)
  // If we have started configuration then the DataTypeManager will wait until
  // the current configuration is done before processing the reconfigure
  // request. We goto IDLE state and clear all our internal state. It is
  // safe to do this as we have not started association on any DTCs.
  void ResetForReconfiguration();

  // Should only be called after Initialize.
  // Stops any disabled types.
  void StopDisabledTypes();

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
    // A stop command was issued.
    ABORTED,
    // No configuration is in progress.
    IDLE
  };

  // Returns true if any requested types currently need to start model
  // association.  If non-null, fills |needs_start| with all such controllers.
  bool GetControllersNeedingStart(
      std::vector<DataTypeController*>* needs_start);

  // Callback passed to each data type controller on starting association. This
  // callback will be invoked when the model association is done.
  void TypeStartCallback(DataTypeController::StartResult start_result,
                         const syncer::SyncMergeResult& local_merge_result,
                         const syncer::SyncMergeResult& syncer_merge_result);

  // Callback that will be invoked when the models finish loading. This callback
  // will be passed to |LoadModels| function.
  void ModelLoadCallback(syncer::ModelType type, syncer::SyncError error);

  // Calls the |LoadModels| method on the next controller waiting to start.
  void LoadModelForNextType();

  // Calls |StartAssociating| on the next available controller whose models are
  // loaded.
  void StartAssociatingNextType();

  // When a type fails to load or fails associating this method is invoked to
  // do the book keeping and do the UMA reporting.
  void AppendToFailedDatatypesAndLogError(
      DataTypeController::StartResult result,
      const syncer::SyncError& error);

  syncer::ModelTypeSet GetTypesWaitingToLoad();


  State state_;
  syncer::ModelTypeSet desired_types_;
  std::list<syncer::SyncError> failed_datatypes_info_;
  std::map<syncer::ModelType, int> start_order_;

  // This illustration explains the movement of one DTC through various lists.
  // Consider a dataype, say, BOOKMARKS which is NOT_RUNNING and will be
  // configured now.
  // Initially all lists are empty. BOOKMARKS is in the |controllers_|
  // map. Here is how the controller moves to various list
  // (indicated by arrows). The first column is the method that causes the
  // transition.
  // Step 1 : |Initialize| - |controllers_| -> |needs_start_|
  // Step 2 : |LoadModelForNextType| - |needs_start_| -> |pending_model_load_|
  // Step 3 : |ModelLoadCallback| - |pending_model_load_| ->
  //    |waiting_to_associate_|
  // Step 4 : |StartAssociatingNextType| - |waiting_to_associate_| ->
  //    |currently_associating_|
  // Step 5 : |TypeStartCallback| - |currently_associating_| set to NULL.

  // Controllers that need to be started during a config cycle.
  std::vector<DataTypeController*> needs_start_;

  // Controllers that need to be stopped during a config cycle.
  std::vector<DataTypeController*> needs_stop_;

  // Controllers whose |LoadModels| function has been invoked and that are
  // waiting for their models to be loaded. Cotrollers will be moved from
  // |needs_start_| to this list as their |LoadModels| method is invoked.
  std::vector<DataTypeController*> pending_model_load_;

  // Controllers whose models are loaded and are ready to do model
  // association. Controllers will be moved from |pending_model_load_|
  // list to this list as they finish loading their model.
  std::vector<DataTypeController*> waiting_to_associate_;

  // Controller currently doing model association.
  DataTypeController* currently_associating_;

  // Set of all registered controllers.
  const DataTypeController::TypeMap* controllers_;

  // The processor in charge of handling model association results.
  ModelAssociationResultProcessor* result_processor_;

  // Timer to track and limit how long a datatype takes to model associate.
  base::OneShotTimer<ModelAssociationManager> timer_;

  // Sync's datatype debug info listener, which we pass model association
  // statistics to.
  const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>
      debug_info_listener_;

  base::WeakPtrFactory<ModelAssociationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModelAssociationManager);
};
}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
