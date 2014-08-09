// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/sync_driver/non_ui_data_type_controller.h"

class Profile;
class ProfileSyncComponentsFactory;

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace browser_sync {

class AutofillProfileDataTypeController
    : public sync_driver::NonUIDataTypeController,
      public autofill::PersonalDataManagerObserver {
 public:
  AutofillProfileDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);

  // NonUIDataTypeController implementation.
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;

  // PersonalDataManagerObserver implementation:
  virtual void OnPersonalDataChanged() OVERRIDE;

 protected:
  virtual ~AutofillProfileDataTypeController();

  // NonUIDataTypeController implementation.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual bool StartModels() OVERRIDE;
  virtual void StopModels() OVERRIDE;

 private:
  // Callback to notify that WebDatabase has loaded.
  void WebDatabaseLoaded();

  Profile* const profile_;
  autofill::PersonalDataManager* personal_data_;
  bool callback_registered_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
