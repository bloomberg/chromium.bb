// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/glue/data_type_controller.h"

class PasswordStore;
class Profile;
class ProfileSyncFactory;
class ProfileSyncService;

namespace browser_sync {

class AssociatorInterface;
class ChangeProcessor;
class ControlTask;

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public DataTypeController {
 public:
  PasswordDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~PasswordDataTypeController();

  // DataTypeController implementation
  virtual void Start(StartCallback* start_callback);

  virtual void Stop();

  virtual bool enabled();

  virtual syncable::ModelType type() const;

  virtual browser_sync::ModelSafeGroup model_safe_group() const;

  virtual std::string name() const;

  virtual State state() const;

  // UnrecoverableHandler implementation
  virtual void OnUnrecoverableError(const tracked_objects::Location& from_here,
                                    const std::string& message);

 private:
  void StartImpl();
  void StartDone(StartResult result, State state);
  void StartDoneImpl(StartResult result, State state);
  void StopImpl();
  void StartFailed(StartResult result);
  void OnUnrecoverableErrorImpl(const tracked_objects::Location& from_here,
                                const std::string& message);

  void set_state(State state) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    state_ = state;
  }

  ProfileSyncFactory* profile_sync_factory_;
  Profile* profile_;
  ProfileSyncService* sync_service_;
  State state_;

  scoped_ptr<AssociatorInterface> model_associator_;
  scoped_ptr<ChangeProcessor> change_processor_;
  scoped_ptr<StartCallback> start_callback_;
  scoped_refptr<PasswordStore> password_store_;

  base::Lock abort_association_lock_;
  bool abort_association_;
  base::WaitableEvent abort_association_complete_;

  // Barrier to ensure that the datatype has been stopped on the DB thread
  // from the UI thread.
  base::WaitableEvent datatype_stopped_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
