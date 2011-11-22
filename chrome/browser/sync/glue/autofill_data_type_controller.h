// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/glue/new_non_frontend_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class WebDataService;

namespace browser_sync {

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController : public NewNonFrontendDataTypeController,
                                   public content::NotificationObserver {
 public:
  AutofillDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);
  virtual ~AutofillDataTypeController();

  // NewNonFrontendDataTypeController implementation.
  virtual syncable::ModelType type() const OVERRIDE;
  virtual browser_sync::ModelSafeGroup model_safe_group() const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int notification_type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  // NewNonFrontendDataTypeController implementation.
  virtual bool StartModels() OVERRIDE;
  virtual bool StartAssociationAsync() OVERRIDE;
  virtual base::WeakPtr<SyncableService> GetWeakPtrToSyncableService()
      const OVERRIDE;
  virtual void StopModels() OVERRIDE;
  virtual void StopLocalServiceAsync() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

 private:
  friend class AutofillDataTypeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSReady);
  FRIEND_TEST_ALL_PREFIXES(AutofillDataTypeControllerTest, StartWDSNotReady);

  scoped_refptr<WebDataService> web_data_service_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
