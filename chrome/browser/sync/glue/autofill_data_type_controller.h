// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/glue/non_ui_data_type_controller.h"
#include "chrome/browser/webdata/autofill_web_data_service_observer.h"

class AutofillWebDataService;

namespace browser_sync {

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController
    : public NonUIDataTypeController,
      public AutofillWebDataServiceObserverOnUIThread {
 public:
  AutofillDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);

  // NonUIDataTypeController implementation.
  virtual syncer::ModelType type() const OVERRIDE;
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE;

  // NonFrontendDatatypeController override, needed as stop-gap until bug
  // 163431 is addressed / implemented.
  virtual void StartAssociating(const StartCallback& start_callback) OVERRIDE;

  // AutofillWebDataServiceObserverOnUIThread implementation.
  virtual void WebDatabaseLoaded() OVERRIDE;

 protected:
  virtual ~AutofillDataTypeController();

  // NonUIDataTypeController implementation.
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual bool StartModels() OVERRIDE;
  virtual void StopModels() OVERRIDE;

 private:
  friend class AutofillDataTypeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSReady);
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSNotReady);

  // Self-invoked on the DB thread to call the AutocompleteSyncableService with
  // an updated value of autofill culling settings.
  void UpdateAutofillCullingSettings(bool cull_expired_entries);

  scoped_refptr<AutofillWebDataService> web_data_service_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
