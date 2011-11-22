// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_APP_NOTIFICATION_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_APP_NOTIFICATION_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AppNotificationManager;

namespace browser_sync {

class AppNotificationDataTypeController
    : public FrontendDataTypeController,
      public content::NotificationObserver {
 public:
  AppNotificationDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~AppNotificationDataTypeController();

  // FrontendDataTypeController implementation.
  virtual syncable::ModelType type() const OVERRIDE;

  // NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden in test to control creation and init order.
  virtual AppNotificationManager* GetAppNotificationManager();

 private:
  // FrontendDataTypeController implementations.
  virtual bool StartModels() OVERRIDE;
  virtual void CleanUpState() OVERRIDE;
  virtual void CreateSyncComponents() OVERRIDE;
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE;
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE;
  virtual void RecordStartFailure(StartResult result) OVERRIDE;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_APP_NOTIFICATION_DATA_TYPE_CONTROLLER_H__
