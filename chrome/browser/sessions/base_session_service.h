// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/sessions/session_id.h"
#include "url/gurl.h"

class Profile;
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
  // |type| gives the type of session service, |profile| the profile and
  // |path| the path to save files to. If |profile| is non-NULL, |path| is
  // ignored and instead the path comes from the profile.
  BaseSessionService(SessionType type,
                     Profile* profile,
                     const base::FilePath& path);

  Profile* profile() const { return profile_; }

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

  // Creates a SessionCommand that represents a navigation.
  SessionCommand* CreateUpdateTabNavigationCommand(
      SessionID::id_type command_id,
      SessionID::id_type tab_id,
      const sessions::SerializedNavigationEntry& navigation);

  // Creates a SessionCommand that represents marking a tab as an application.
  SessionCommand* CreateSetTabExtensionAppIDCommand(
      SessionID::id_type command_id,
      SessionID::id_type tab_id,
      const std::string& extension_id);

  // Creates a SessionCommand that containing user agent override used by a
  // tab's navigations.
  SessionCommand* CreateSetTabUserAgentOverrideCommand(
      SessionID::id_type command_id,
      SessionID::id_type tab_id,
      const std::string& user_agent_override);

  // Creates a SessionCommand stores a browser window's app name.
  SessionCommand* CreateSetWindowAppNameCommand(
      SessionID::id_type command_id,
      SessionID::id_type window_id,
      const std::string& app_name);

  // Converts a SessionCommand previously created by
  // CreateUpdateTabNavigationCommand into a
  // sessions::SerializedNavigationEntry. Returns true on success. If
  // successful |tab_id| is set to the id of the restored tab.
  bool RestoreUpdateTabNavigationCommand(
      const SessionCommand& command,
      sessions::SerializedNavigationEntry* navigation,
      SessionID::id_type* tab_id);

  // Extracts a SessionCommand as previously created by
  // CreateSetTabExtensionAppIDCommand into the tab id and application
  // extension id.
  bool RestoreSetTabExtensionAppIDCommand(
      const SessionCommand& command,
      SessionID::id_type* tab_id,
      std::string* extension_app_id);

  // Extracts a SessionCommand as previously created by
  // CreateSetTabUserAgentOverrideCommand into the tab id and user agent.
  bool RestoreSetTabUserAgentOverrideCommand(
      const SessionCommand& command,
      SessionID::id_type* tab_id,
      std::string* user_agent_override);

  // Extracts a SessionCommand as previously created by
  // CreateSetWindowAppNameCommand into the window id and application name.
  bool RestoreSetWindowAppNameCommand(
      const SessionCommand& command,
      SessionID::id_type* window_id,
      std::string* app_name);

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
  bool RunTaskOnBackendThread(const tracked_objects::Location& from_here,
                              const base::Closure& task);

  // Max number of navigation entries in each direction we'll persist.
  static const int max_persist_navigation_count;

 private:
  friend class BetterSessionRestoreCrashTest;

  // The profile. This may be null during testing.
  Profile* profile_;

  // The backend.
  scoped_refptr<SessionBackend> backend_;

  // Used to invoke Save.
  base::WeakPtrFactory<BaseSessionService> weak_factory_;

  // Commands we need to send over to the backend.
  std::vector<SessionCommand*>  pending_commands_;

  // Whether the backend file should be recreated the next time we send
  // over the commands.
  bool pending_reset_;

  // The number of commands sent to the backend before doing a reset.
  int commands_since_reset_;

  // A token to make sure that all tasks will be serialized.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

  DISALLOW_COPY_AND_ASSIGN(BaseSessionService);
};

#endif  // CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_H_
