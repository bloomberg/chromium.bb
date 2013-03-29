// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "chrome/browser/sync/glue/non_ui_data_type_controller.h"
#include "chrome/browser/webdata/autofill_web_data_service_observer.h"
#include "components/autofill/browser/personal_data_manager_observer.h"

class AutofillWebDataService;
class PersonalDataManager;

namespace browser_sync {

class AutofillProfileDataTypeController
    : public NonUIDataTypeController,
      public AutofillWebDataServiceObserverOnUIThread,
      public PersonalDataManagerObserver {
 public:
  AutofillProfileDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // NonUIDataTypeController implementation.
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;

  // AutofillWebDataServiceObserverOnUIThread implementation.
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
  PersonalDataManager* personal_data_;
  scoped_refptr<AutofillWebDataService> web_data_service_;
  ScopedObserver<AutofillWebDataService, AutofillProfileDataTypeController>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_PROFILE_DATA_TYPE_CONTROLLER_H_
