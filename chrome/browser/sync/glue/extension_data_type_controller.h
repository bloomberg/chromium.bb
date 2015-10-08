// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_TYPE_CONTROLLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "components/sync_driver/generic_change_processor.h"
#include "components/sync_driver/ui_data_type_controller.h"

class Profile;

namespace browser_sync {

// TODO(zea): Rename this and ExtensionSettingsDTC to ExtensionOrApp*, since
// both actually handle the APP datatypes as well.
class ExtensionDataTypeController : public sync_driver::UIDataTypeController {
 public:
  ExtensionDataTypeController(
      syncer::ModelType type,  // Either EXTENSIONS or APPS.
      const base::Closure& error_callback,
      sync_driver::SyncClient* sync_client,
      Profile* profile);

 private:
  ~ExtensionDataTypeController() override;

  // DataTypeController implementations.
  bool StartModels() override;

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_TYPE_CONTROLLER_H_
