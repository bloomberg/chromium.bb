// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_PROXY_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_PROXY_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "components/sync_driver/data_type_controller.h"

namespace sync_driver {

// Implementation for proxy datatypes. These are datatype that have no
// representation in sync, and therefore no change processor or syncable
// service.
class ProxyDataTypeController : public DataTypeController {
 public:
  explicit ProxyDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      syncer::ModelType type);

  // DataTypeController interface.
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void Stop() override;
  syncer::ModelType type() const override;
  std::string name() const override;
  State state() const override;
  void ActivateDataType(BackendDataTypeConfigurer* configurer) override;
  void DeactivateDataType(BackendDataTypeConfigurer* configurer) override;

  // DataTypeErrorHandler interface.
  void OnSingleDataTypeUnrecoverableError(
      const syncer::SyncError& error) override;

 protected:
  // DataTypeController is RefCounted.
  ~ProxyDataTypeController() override;

 private:
  State state_;

  // The actual type for this controller.
  syncer::ModelType type_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDataTypeController);
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_PROXY_DATA_TYPE_CONTROLLER_H__
