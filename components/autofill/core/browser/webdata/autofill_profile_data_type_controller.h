// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/async_directory_type_controller.h"

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}  // namespace autofill

namespace syncer {
class SyncClient;
class SyncService;
}  // namespace syncer

namespace browser_sync {

// Controls syncing of the AUTOFILL_PROFILE data type.
class AutofillProfileDataTypeController
    : public syncer::AsyncDirectoryTypeController,
      public autofill::PersonalDataManagerObserver {
 public:
  using PersonalDataManagerProvider =
      base::RepeatingCallback<autofill::PersonalDataManager*()>;

  // |dump_stack| is called when an unrecoverable error occurs.
  AutofillProfileDataTypeController(
      scoped_refptr<base::SequencedTaskRunner> db_thread,
      const base::Closure& dump_stack,
      syncer::SyncService* sync_service,
      syncer::SyncClient* sync_client,
      const PersonalDataManagerProvider& pdm_provider,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service);
  ~AutofillProfileDataTypeController() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

 protected:
  // AsyncDirectoryTypeController:
  bool StartModels() override;
  void StopModels() override;
  bool ReadyForStart() const override;

 private:
  // Callback to notify that WebDatabase has loaded.
  void WebDatabaseLoaded();

  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  // Returns true if the pref is set such that autofill sync should be enabled.
  bool IsEnabled();

  // Callback that allows accessing PersonalDataManager lazily.
  const PersonalDataManagerProvider pdm_provider_;

  // A reference to the AutofillWebDataService for this controller.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  // Whether the database loaded callback has been registered.
  bool callback_registered_;

  // Registrar for listening to kAutofillWEnabled status.
  PrefChangeRegistrar pref_registrar_;

  // Stores whether we're currently syncing autofill data. This is the last
  // value computed by IsEnabled.
  bool currently_enabled_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
