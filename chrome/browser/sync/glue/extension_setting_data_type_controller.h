// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

class Profile;

namespace extensions {
class StorageFrontend;
}

namespace sync_driver {
class SyncClient;
}

namespace browser_sync {

class ExtensionSettingDataTypeController
    : public sync_driver::NonUIDataTypeController {
 public:
  ExtensionSettingDataTypeController(
      // Either EXTENSION_SETTINGS or APP_SETTINGS.
      syncer::ModelType type,
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client,
      Profile* profile);

  // NonFrontendDataTypeController implementation
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;

 private:
  ~ExtensionSettingDataTypeController() override;

  // NonFrontendDataTypeController implementation.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;

  // Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncer::ModelType type_;

  // Only used on the UI thread.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_DATA_TYPE_CONTROLLER_H__
