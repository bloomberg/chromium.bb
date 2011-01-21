// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class Profile;
class ProfileSyncFactory;
class ProfileSyncService;

namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;

// A class that manages the startup and shutdown of autofill sync.
class AutofillDataTypeController : public DataTypeController,
                                   public NotificationObserver,
                                   public PersonalDataManager::Observer {
 public:
  AutofillDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~AutofillDataTypeController();

  // DataTypeController implementation
  virtual void Start(StartCallback* start_callback);

  virtual void Stop();

  virtual bool enabled();

  virtual syncable::ModelType type();

  virtual browser_sync::ModelSafeGroup model_safe_group();

  virtual const char* name() const;

  virtual State state();

  // UnrecoverableHandler implementation
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // PersonalDataManager::Observer implementation:
  virtual void OnPersonalDataLoaded();

 protected:
  virtual ProfileSyncFactory::SyncComponents CreateSyncComponents(
      ProfileSyncService* profile_sync_service,
      WebDatabase* web_database,
      PersonalDataManager* personal_data,
      browser_sync::UnrecoverableErrorHandler* error_handler);
  ProfileSyncFactory* profile_sync_factory_;

 private:
  void StartImpl();
  void StartDone(StartResult result, State state);
  void StartDoneImpl(StartResult result, State state);
  void StopImpl();
  void StartFailed(StartResult result);
  void OnUnrecoverableErrorImpl(const tracked_objects::Location& from_here,
                                const std::string& message);

  // Second-half of "Start" implementation, called once personal data has
  // loaded.
  void ContinueStartAfterPersonalDataLoaded();

  void set_state(State state) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    state_ = state;
  }

  Profile* profile_;
  ProfileSyncService* sync_service_;
  State state_;

  PersonalDataManager* personal_data_;
  scoped_refptr<WebDataService> web_data_service_;
  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;
  scoped_ptr<StartCallback> start_callback_;

  NotificationRegistrar notification_registrar_;

  base::Lock abort_association_lock_;
  bool abort_association_;
  base::WaitableEvent abort_association_complete_;

  // Barrier to ensure that the datatype has been stopped on the DB thread
  // from the UI thread.
  base::WaitableEvent datatype_stopped_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_AUTOFILL_DATA_TYPE_CONTROLLER_H__
