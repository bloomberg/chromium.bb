// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
#pragma once

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "chrome/common/extensions/user_script.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace base {
class StringPiece;
}

class Profile;

// Manages a segment of shared memory that contains the user scripts the user
// has installed.  Lives on the UI thread.
class UserScriptMaster : public base::RefCountedThreadSafe<UserScriptMaster>,
                         public NotificationObserver {
 public:
  // For testability, the constructor takes the path the scripts live in.
  // This is normally a directory inside the profile.
  explicit UserScriptMaster(const FilePath& script_dir, Profile* profile);

  // Kicks off a process on the file thread to reload scripts from disk
  // into a new chunk of shared memory and notify renderers.
  virtual void StartScan();

  // Gets the segment of shared memory for the scripts.
  base::SharedMemory* GetSharedMemory() const {
    return shared_memory_.get();
  }

  // Called by the script reloader when new scripts have been loaded.
  void NewScriptsAvailable(base::SharedMemory* handle);

  // Return true if we have any scripts ready.
  bool ScriptsReady() const { return shared_memory_.get() != NULL; }

  // Returns the path to the directory user scripts are stored in.
  FilePath user_script_dir() const { return user_script_dir_; }

 protected:
  friend class base::RefCountedThreadSafe<UserScriptMaster>;

  virtual ~UserScriptMaster();

 private:
  FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, Parse1);
  FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, Parse2);
  FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, Parse3);
  FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, Parse4);
  FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, Parse5);
  FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, Parse6);

 public:
  // We reload user scripts on the file thread to prevent blocking the UI.
  // ScriptReloader lives on the file thread and does the reload
  // work, and then sends a message back to its master with a new SharedMemory*.
  // ScriptReloader is the worker that manages running the script scan
  // on the file thread. It must be created on, and its public API must only be
  // called from, the master's thread.
  class ScriptReloader
      : public base::RefCountedThreadSafe<UserScriptMaster::ScriptReloader> {
   public:
    // Parses the includes out of |script| and returns them in |includes|.
    static bool ParseMetadataHeader(const base::StringPiece& script_text,
                                    UserScript* script);

    static void LoadScriptsFromDirectory(const FilePath& script_dir,
                                         UserScriptList* result);

    explicit ScriptReloader(UserScriptMaster* master);

    // Start a scan for scripts.
    // Will always send a message to the master upon completion.
    void StartScan(const FilePath& script_dir,
                   const UserScriptList& external_scripts);

    // The master is going away; don't call it back.
    void DisownMaster() {
      master_ = NULL;
    }

   private:
    friend class base::RefCountedThreadSafe<UserScriptMaster::ScriptReloader>;

    ~ScriptReloader() {}

    // Where functions are run:
    //    master          file
    //   StartScan   ->  RunScan
    //                     LoadScriptsFromDirectory()
    //                     LoadLoneScripts()
    // NotifyMaster  <-  RunScan

    // Runs on the master thread.
    // Notify the master that new scripts are available.
    void NotifyMaster(base::SharedMemory* memory);

    // Runs on the File thread.
    // Scan the specified directory and lone scripts, calling NotifyMaster when
    // done. The parameters are intentionally passed by value so their lifetimes
    // aren't tied to the caller.
    void RunScan(const FilePath script_dir, UserScriptList lone_scripts);

    // A pointer back to our master.
    // May be NULL if DisownMaster() is called.
    UserScriptMaster* master_;

    // The message loop to call our master back on.
    // Expected to always outlive us.
    BrowserThread::ID master_thread_id_;

    DISALLOW_COPY_AND_ASSIGN(ScriptReloader);
  };

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Manages our notification registrations.
  NotificationRegistrar registrar_;

  // The directories containing user scripts.
  FilePath user_script_dir_;

  // We hang on to our pointer to know if we've already got one running.
  scoped_refptr<ScriptReloader> script_reloader_;

  // Contains the scripts that were found the last time scripts were updated.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // List of scripts outside of script directories we should also load.
  UserScriptList lone_scripts_;

  // If the extensions service has finished loading its initial set of
  // extensions.
  bool extensions_service_ready_;

  // If the script directory is modified while we're rescanning it, we note
  // that we're currently mid-scan and then start over again once the scan
  // finishes.  This boolean tracks whether another scan is pending.
  bool pending_scan_;

  // The profile for which the scripts managed here are installed.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptMaster);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
