// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "url/gurl.h"

class BaseSessionServiceDelegate;
class SessionBackend;
class SessionCommand;

namespace sessions {
class SerializedNavigationEntry;
}

// BaseSessionService is the super class of both tab restore service and
// session service. It contains commonality needed by both, in particular
// it manages a set of SessionCommands that are periodically sent to a
// SessionBackend.
class BaseSessionService {
 public:
  // Identifies the type of session service this is. This is used by the
  // backend to determine the name of the files.
  enum SessionType {
    SESSION_RESTORE,
    TAB_RESTORE
  };

  // Creates a new BaseSessionService. After creation you need to invoke
  // Init.
  // |type| gives the type of session service, |path| the path to save files to.
  BaseSessionService(SessionType type,
                     const base::FilePath& path,
                     scoped_ptr<BaseSessionServiceDelegate> delegate);

  // Deletes the last session.
  void DeleteLastSession();

  typedef base::Callback<void(ScopedVector<SessionCommand>)>
      InternalGetCommandsCallback;

 protected:
  virtual ~BaseSessionService();

  // Returns the backend.
  SessionBackend* backend() const { return backend_.get(); }

  // Returns the set of commands that needed to be scheduled. The commands
  // in the vector are owned by BaseSessionService, until they are scheduled
  // on the backend at which point the backend owns the commands.
  std::vector<SessionCommand*>&  pending_commands() {
    return pending_commands_;
  }

  // Whether the next save resets the file before writing to it.
  void set_pending_reset(bool value) { pending_reset_ = value; }
  bool pending_reset() const { return pending_reset_; }

  // Returns the number of commands sent down since the last reset.
  int commands_since_reset() const { return commands_since_reset_; }

  // Schedules a command. This adds |command| to pending_commands_ and
  // invokes StartSaveTimer to start a timer that invokes Save at a later
  // time.
  virtual void ScheduleCommand(SessionCommand* command);

  // Starts the timer that invokes Save (if timer isn't already running).
  void StartSaveTimer();

  // Saves pending commands to the backend. This is invoked from the timer
  // scheduled by StartSaveTimer.
  virtual void Save();

  // Returns true if the entry at specified |url| should be written to disk.
  bool ShouldTrackEntry(const GURL& url);

  // Invokes SessionBackend::ReadLastSessionCommands with callback on the
  // backend thread.
  // If testing, SessionBackend::ReadLastSessionCommands is invoked directly.
  base::CancelableTaskTracker::TaskId ScheduleGetLastSessionCommands(
      const InternalGetCommandsCallback& callback,
      base::CancelableTaskTracker* tracker);

  // This posts the task to the SequencedWorkerPool, or run immediately
  // if the SequencedWorkerPool has been shutdown.
  void RunTaskOnBackendThread(const tracked_objects::Location& from_here,
                              const base::Closure& task);

  // Max number of navigation entries in each direction we'll persist.
  static const int max_persist_navigation_count;

 private:
  friend class BetterSessionRestoreCrashTest;

  // The backend.
  scoped_refptr<SessionBackend> backend_;

  // Commands we need to send over to the backend.
  std::vector<SessionCommand*>  pending_commands_;

  // Whether the backend file should be recreated the next time we send
  // over the commands.
  bool pending_reset_;

  // The number of commands sent to the backend before doing a reset.
  int commands_since_reset_;

  scoped_ptr<BaseSessionServiceDelegate> delegate_;

  // A token to make sure that all tasks will be serialized.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

  // Used to invoke Save.
  base::WeakPtrFactory<BaseSessionService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BaseSessionService);
};

#endif  // CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_
