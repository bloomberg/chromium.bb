// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__

#include <memory>

#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/syncable/directory.h"

namespace syncer {

class ChangeProcessor;

// Base class for Directory based Data type controllers.
class DirectoryDataTypeController : public DataTypeController {
 public:
  ~DirectoryDataTypeController() override;

  // DataTypeController implementation.
  bool ShouldLoadModelBeforeConfigure() const override;
  void GetAllNodes(const AllNodesCallback& callback) override;
  void GetStatusCounters(const StatusCountersCallback& callback) override;

  // Directory based data types don't need to register with backend.
  // ModelTypeRegistry will create all necessary objects in
  // SetEnabledDirectoryTypes based on routing info.
  void RegisterWithBackend(BackendDataTypeConfigurer* configurer) override;

  // Directory specific implementation of ActivateDataType with the
  // type specific ChangeProcessor and ModelSafeGroup.
  // Activates change processing on the controlled data type.
  // This is called by DataTypeManager, synchronously with data type's
  // model association.
  // See BackendDataTypeConfigurer::ActivateDataType for more details.
  void ActivateDataType(BackendDataTypeConfigurer* configurer) override;

  // Directory specific implementation of DeactivateDataType.
  // Deactivates change processing on the controlled data type (by removing
  // the data type's ChangeProcessor registration with the backend).
  // See BackendDataTypeConfigurer::DeactivateDataType for more details.
  void DeactivateDataType(BackendDataTypeConfigurer* configurer) override;

  // Returns a ListValue representing all nodes for a specified type by querying
  // the directory.
  static std::unique_ptr<base::ListValue> GetAllNodesForTypeFromDirectory(
      ModelType type,
      syncable::Directory* directory);

 protected:
  // |dump_stack| is called when an unrecoverable error occurs.
  DirectoryDataTypeController(ModelType type,
                              const base::Closure& dump_stack,
                              SyncClient* sync_client,
                              ModelSafeGroup model_safe_group);

  // Access to the ChangeProcessor for the type being controlled by |this|.
  // Returns null if the ChangeProcessor isn't created or connected.
  virtual ChangeProcessor* GetChangeProcessor() const = 0;

  SyncClient* const sync_client_;

 private:
  // The model safe group of this data type.  This should reflect the
  // thread that should be used to modify the data type's native
  // model.
  ModelSafeGroup model_safe_group_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__
