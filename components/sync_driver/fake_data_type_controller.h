// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/directory_data_type_controller.h"

namespace sync_driver {

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
  explicit FakeDataTypeController(syncer::ModelType type);

  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void Stop() override;
  syncer::ModelType type() const override;
  std::string name() const override;
  syncer::ModelSafeGroup model_safe_group() const override;
  ChangeProcessor* GetChangeProcessor() const override;
  State state() const override;
  void OnSingleDataTypeUnrecoverableError(
      const syncer::SyncError& error) override;
  bool ReadyForStart() const override;

  void FinishStart(ConfigureResult result);

  void SetDelayModelLoad();

  void SetModelLoadError(syncer::SyncError error);

  void SimulateModelLoadFinishing();

  void SetReadyForStart(bool ready);

 protected:
  ~FakeDataTypeController() override;

 private:
  DataTypeController::State state_;
  bool model_load_delayed_;
  syncer::ModelType type_;
  StartCallback last_start_callback_;
  ModelLoadCallback model_load_callback_;
  syncer::SyncError load_error_;
  bool ready_for_start_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_DATA_TYPE_CONTROLLER_H__
