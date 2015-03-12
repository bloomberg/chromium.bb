// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_

#include "base/basictypes.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

class Profile;
class ProfileSyncComponentsFactory;

namespace browser_sync {

class AutofillWalletDataTypeController
    : public sync_driver::NonUIDataTypeController {
 public:
  AutofillWalletDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);

  // NonUIDataTypeController implementation.
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;

 protected:
  ~AutofillWalletDataTypeController() override;

 private:
  // NonUIDataTypeController implementation.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;
  void StopModels() override;

  void WebDatabaseLoaded();

  Profile* const profile_;
  bool callback_registered_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
