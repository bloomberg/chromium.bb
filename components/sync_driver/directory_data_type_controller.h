// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__
#define COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__

#include "components/sync_driver/data_type_controller.h"

#include "sync/internal_api/public/engine/model_safe_worker.h"

namespace sync_driver {
class ChangeProcessor;

// Base class for Directory based Data type controllers.
class DirectoryDataTypeController : public DataTypeController {
 public:
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

 protected:
  // The model safe group of this data type.  This should reflect the
  // thread that should be used to modify the data type's native
  // model.
  virtual syncer::ModelSafeGroup model_safe_group() const = 0;

  // Access to the ChangeProcessor for the type being controlled by |this|.
  // Returns NULL if the ChangeProcessor isn't created or connected.
  virtual ChangeProcessor* GetChangeProcessor() const = 0;

  DirectoryDataTypeController(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const base::Closure& error_callback);

  ~DirectoryDataTypeController() override;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DIRECTORY_DATA_TYPE_CONTROLLER_H__
