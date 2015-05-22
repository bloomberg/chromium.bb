// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

class Profile;
class ProfileSyncComponentsFactory;

namespace browser_sync {

// Controls syncing of either AUTOFILL_WALLET or AUTOFILL_WALLET_METADATA.
class AutofillWalletDataTypeController
    : public sync_driver::NonUIDataTypeController {
 public:
  // |model_type| should be either AUTOFILL_WALLET or AUTOFILL_WALLET_METADATA.
  AutofillWalletDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      syncer::ModelType model_type);

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
  bool ReadyForStart() const override;

  // Callback for changes to the autofill prefs.
  void OnSyncPrefChanged();

  // Returns true if the prefs are set such that wallet sync should be enabled.
  bool IsEnabled();

  Profile* const profile_;
  bool callback_registered_;
  syncer::ModelType model_type_;

  // Stores whether we're currently syncing wallet data. This is the last
  // value computed by IsEnabled.
  bool currently_enabled_;

  // Registrar for listening to kAutofillWalletSyncExperimentEnabled status.
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
