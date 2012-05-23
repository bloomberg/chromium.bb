// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
#pragma once

#include <list>

#include "base/memory/weak_ptr.h"

#include "chrome/browser/sync/glue/data_type_manager.h"

// |ModelAssociationManager| does the heavy lifting for doing the actual model
// association. It instructs DataTypeControllers to load models, start
// associating and stopping. Since the operations are async it uses an
// interface to inform DTM the results of the operations.
// This class is owned by DTM.
namespace browser_sync {

class DataTypeController;

// |ModelAssociationManager| association functions are async. The results of
// those operations are passed back via this interface.
class ModelAssociationResultProcessor {
 public:
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
  // of Initialize is only allowed if the current configure cycle is completed,
  // aborted or stopped.
  void Initialize(syncable::ModelTypeSet desired_types);

  // Can be called at any time. Synchronously stops all datatypes.
  void Stop();

  // Should only be called after Initialize.
  // Starts the actual association. When this is completed
  // |OnModelAssociationDone| would be called invoked.
  void StartAssociationAsync();

  // It is valid to call this only when we are initialized to configure
  // but we have not started the configuration.(i.e., |Initialize| has
  // been called but |StartAssociationAsync| has not yet been called.)
  // If we have started configuration then the DTM will wait until
  // the current configuration is done before processing the reconfigure
  // request. We goto IDLE state and clear all our internal state. It is
  // safe to do this as we have not started association on any DTCs.
  void ResetForReconfiguration();

  // Should only be called after Initialize.
  // Stops any disabled types.
  void StopDisabledTypes();

 private:
  enum State {
    // This is the state after |Initialize| is called.
    INITIAILIZED_TO_CONFIGURE,
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

  // Callback passed to each data type controller on startup.
  void TypeStartCallback(DataTypeController::StartResult result,
                         const SyncError& error);
  void StartNextType();

  State state_;
  syncable::ModelTypeSet desired_types_;
  std::list<SyncError> failed_datatypes_info_;
  std::map<syncable::ModelType, int> start_order_;
  std::vector<DataTypeController*> needs_start_;
  std::vector<DataTypeController*> needs_stop_;
  const DataTypeController::TypeMap* controllers_;
  ModelAssociationResultProcessor* result_processor_;
  base::WeakPtrFactory<ModelAssociationManager> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ModelAssociationManager);
};
}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATION_MANAGER_H__
