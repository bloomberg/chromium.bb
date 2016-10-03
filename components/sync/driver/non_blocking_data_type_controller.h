// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_prefs.h"

namespace syncer {

class SyncClient;
struct ActivationContext;

// Base class for DataType controllers for Unified Sync and Storage datatypes.
// Derived types must implement the following methods:
// - RunOnModelThread
// - RunOnUIThread
class NonBlockingDataTypeController : public DataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  NonBlockingDataTypeController(ModelType type,
                                const base::Closure& dump_stack,
                                SyncClient* sync_client);
  ~NonBlockingDataTypeController() override;

  // DataTypeController implementation.
  bool ShouldLoadModelBeforeConfigure() const override;
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void GetAllNodes(const AllNodesCallback& callback) override;

  // Registers non-blocking data type with sync backend. In the process the
  // activation context is passed to ModelTypeRegistry, where ModelTypeWorker
  // gets created and connected with ModelTypeProcessor.
  void RegisterWithBackend(BackendDataTypeConfigurer* configurer) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void ActivateDataType(BackendDataTypeConfigurer* configurer) override;
  void DeactivateDataType(BackendDataTypeConfigurer* configurer) override;
  void Stop() override;
  std::string name() const override;
  State state() const override;

 protected:
  // Posts the given task to the model thread, i.e. the thread the
  // datatype lives on.  Return value: True if task posted successfully,
  // false otherwise.
  virtual bool RunOnModelThread(const tracked_objects::Location& from_here,
                                const base::Closure& task) = 0;

  std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() override;

 private:
  void RecordStartFailure(ConfigureResult result) const;
  void ReportLoadModelError(const SyncError& error);

  // If the DataType controller is waiting for models to load, once the models
  // are loaded this function should be called to let the base class
  // implementation know that it is safe to continue with the activation.
  // The error indicates whether the loading completed successfully.
  void LoadModelsDone(ConfigureResult result, const SyncError& error);

  // The function will do the real work when OnProcessorStarted got called. This
  // is called on the UI thread.
  void OnProcessorStarted(
      SyncError error,
      std::unique_ptr<ActivationContext> activation_context);

  // Sync client
  SyncClient* const sync_client_;

  // Sync prefs. Used for determinig if DisableSync should be called during call
  // to Stop().
  SyncPrefs sync_prefs_;

  // State of this datatype controller.
  State state_;

  // Callbacks for use when starting the datatype.
  ModelLoadCallback model_load_callback_;

  // Controller receives |activation_context_| from SharedModelTypeProcessor
  // callback and must temporarily own it until ActivateDataType is called.
  std::unique_ptr<ActivationContext> activation_context_;

  // This is a hack to prevent reconfigurations from crashing, because USS
  // activation is not idempotent. RegisterWithBackend only needs to actually do
  // something the first time after the type is enabled.
  // TODO(crbug.com/647505): Remove this once the DTM handles things better.
  bool activated_ = false;

  DISALLOW_COPY_AND_ASSIGN(NonBlockingDataTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_NON_BLOCKING_DATA_TYPE_CONTROLLER_H_
