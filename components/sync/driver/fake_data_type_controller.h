// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__

#include <memory>
#include <string>

#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/directory_data_type_controller.h"

namespace syncer {

// Fake DataTypeController implementation that simulates the state
// machine of a typical asynchronous data type.
//
// TODO(akalin): Consider using subclasses of
// {Frontend,NonFrontend,NewNonFrontend}DataTypeController instead, so
// that we don't have to update this class if we change the expected
// behavior of controllers. (It would be easier of the above classes
// used delegation instead of subclassing for per-data-type
// functionality.)
class FakeDataTypeController : public DirectoryDataTypeController {
 public:
  explicit FakeDataTypeController(ModelType type);
  ~FakeDataTypeController() override;

  // DirectoryDataTypeController implementation.
  bool ShouldLoadModelBeforeConfigure() const override;
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void RegisterWithBackend(base::Callback<void(bool)> set_downloaded,
                           ModelTypeConfigurer* configurer) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void Stop() override;
  ChangeProcessor* GetChangeProcessor() const override;
  State state() const override;
  bool ReadyForStart() const override;
  std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() override;

  void FinishStart(ConfigureResult result);

  void SetDelayModelLoad();

  void SetModelLoadError(SyncError error);

  void SimulateModelLoadFinishing();

  void SetReadyForStart(bool ready);

  void SetShouldLoadModelBeforeConfigure(bool value);

  int register_with_backend_call_count() const {
    return register_with_backend_call_count_;
  }

 private:
  DataTypeController::State state_;
  bool model_load_delayed_;
  StartCallback last_start_callback_;
  ModelLoadCallback model_load_callback_;
  SyncError load_error_;
  bool ready_for_start_;
  bool should_load_model_before_configure_;
  int register_with_backend_call_count_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__
