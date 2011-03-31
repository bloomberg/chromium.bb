// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class NotificationDetails;
class NotificationType;
class NotificationSource;

namespace browser_sync {

// A class that manages the startup and shutdown of bookmark sync.
class BookmarkDataTypeController : public FrontendDataTypeController,
                                   public NotificationObserver {
 public:
  BookmarkDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~BookmarkDataTypeController();

  // FrontendDataTypeController interface.
  virtual syncable::ModelType type() const;

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // FrontendDataTypeController interface.
  virtual bool StartModels();
  virtual void CleanupState();
  virtual void CreateSyncComponents();
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);
  virtual void RecordAssociationTime(base::TimeDelta time);
  virtual void RecordStartFailure(StartResult result);

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
