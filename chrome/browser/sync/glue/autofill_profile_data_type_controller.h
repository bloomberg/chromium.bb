// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "chrome/browser/sync/glue/non_ui_data_type_controller.h"
#include "components/autofill/browser/personal_data_manager_observer.h"
#include "components/webdata/common/web_database_observer.h"

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}  // namespace autofill

namespace browser_sync {

class AutofillProfileDataTypeController
    : public NonUIDataTypeController,
      public WebDatabaseObserver,
      public autofill::PersonalDataManagerObserver {
 public:
  AutofillProfileDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // NonUIDataTypeController implementation.
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;

  // WebDatabaseObserver implementation.
  virtual void WebDatabaseLoaded() OVERRIDE;

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
  autofill::PersonalDataManager* personal_data_;
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
