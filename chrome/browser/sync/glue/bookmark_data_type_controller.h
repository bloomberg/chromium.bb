// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class NotificationDetails;
class NotificationType;
class NotificationSource;
class Profile;
class ProfileSyncService;
class ProfileSyncFactory;

namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;

// A class that manages the startup and shutdown of bookmark sync.
class BookmarkDataTypeController : public DataTypeController,
                                   public NotificationObserver {
 public:
  BookmarkDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~BookmarkDataTypeController();

  // DataTypeController interface.
  virtual void Start(StartCallback* start_callback);

  virtual void Stop();

  virtual bool enabled();

  virtual syncable::ModelType type();

  virtual browser_sync::ModelSafeGroup model_safe_group();

  virtual const char* name() const;

  virtual State state();

  // UnrecoverableErrorHandler interface.
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Runs model association and change processor registration.
  void Associate();

  // Helper method to run the stashed start callback with a given result.
  void FinishStart(StartResult result);

  // Cleans up state and calls callback when star fails.
  void StartFailed(StartResult result);

  ProfileSyncFactory* profile_sync_factory_;
  Profile* profile_;
  ProfileSyncService* sync_service_;

  State state_;

  scoped_ptr<StartCallback> start_callback_;
  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_DATA_TYPE_CONTROLLER_H__
