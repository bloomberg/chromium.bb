// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class NotificationDetails;
class NotificationType;
class NotificationSource;

namespace browser_sync {

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController : public NonFrontendDataTypeController,
                                   public NotificationObserver,
                                   public PersonalDataManager::Observer {
 public:
  AutofillDataTypeController(ProfileSyncFactory* profile_sync_factory,
                             Profile* profile);
  virtual ~AutofillDataTypeController();

  // NonFrontendDataTypeController implementation
  virtual syncable::ModelType type() const;
  virtual browser_sync::ModelSafeGroup model_safe_group() const;

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // PersonalDataManager::Observer implementation:
  virtual void OnPersonalDataChanged() OVERRIDE;

 protected:
   // NonFrontendDataTypeController interface.
   virtual bool StartModels();
   virtual bool StartAssociationAsync();
   virtual void CreateSyncComponents();
   virtual void StopModels();
   virtual bool StopAssociationAsync();
   virtual void RecordUnrecoverableError(
       const tracked_objects::Location& from_here,
       const std::string& message);
   virtual void RecordAssociationTime(base::TimeDelta time);
   virtual void RecordStartFailure(StartResult result);

   // Getters and setters
   PersonalDataManager* personal_data() const;
   WebDataService* web_data_service() const;
 private:
  PersonalDataManager* personal_data_;
  scoped_refptr<WebDataService> web_data_service_;
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
