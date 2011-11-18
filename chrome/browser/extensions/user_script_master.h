// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/user_script.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class StringPiece;
}

namespace content {
class RenderProcessHost;
}

class Profile;

typedef std::map<std::string, ExtensionSet::ExtensionPathAndDefaultLocale>
    ExtensionsInfo;

// Manages a segment of shared memory that contains the user scripts the user
// has installed.  Lives on the UI thread.
class UserScriptMaster : public base::RefCountedThreadSafe<UserScriptMaster>,
                         public content::NotificationObserver {
 public:
  explicit UserScriptMaster(Profile* profile);

  // Kicks off a process on the file thread to reload scripts from disk
  // into a new chunk of shared memory and notify renderers.
  virtual void StartLoad();

  // Gets the segment of shared memory for the scripts.
  base::SharedMemory* GetSharedMemory() const {
    return shared_memory_.get();
  }

  // Called by the script reloader when new scripts have been loaded.
  void NewScriptsAvailable(base::SharedMemory* handle);

  // Return true if we have any scripts ready.
  bool ScriptsReady() const { return shared_memory_.get() != NULL; }

 protected:
  friend class base::RefCountedThreadSafe<UserScriptMaster>;

  virtual ~UserScriptMaster();

 public:
  // We reload user scripts on the file thread to prevent blocking the UI.
  // ScriptReloader lives on the file thread and does the reload
  // work, and then sends a message back to its master with a new SharedMemory*.
  // ScriptReloader is the worker that manages running the script load
  // on the file thread. It must be created on, and its public API must only be
  // called from, the master's thread.
  class ScriptReloader
      : public base::RefCountedThreadSafe<UserScriptMaster::ScriptReloader> {
   public:
    // Parses the includes out of |script| and returns them in |includes|.
    static bool ParseMetadataHeader(const base::StringPiece& script_text,
                                    UserScript* script);

    explicit ScriptReloader(UserScriptMaster* master);

    // Start loading of scripts.
    // Will always send a message to the master upon completion.
    void StartLoad(const UserScriptList& external_scripts,
                   const ExtensionsInfo& extension_info_);

    // The master is going away; don't call it back.
    void DisownMaster() {
      master_ = NULL;
    }

   private:
    FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, SkipBOMAtTheBeginning);
    FRIEND_TEST_ALL_PREFIXES(UserScriptMasterTest, LeaveBOMNotAtTheBeginning);
    friend class base::RefCountedThreadSafe<UserScriptMaster::ScriptReloader>;

    ~ScriptReloader() {}

    // Where functions are run:
    //    master          file
    //   StartLoad   ->  RunLoad
    //                     LoadUserScripts()
    // NotifyMaster  <-  RunLoad

    // Runs on the master thread.
    // Notify the master that new scripts are available.
    void NotifyMaster(base::SharedMemory* memory);

    // Runs on the File thread.
    // Load the specified user scripts, calling NotifyMaster when done.
    // |user_scripts| is intentionally passed by value so its lifetime isn't
    // tied to the caller.
    void RunLoad(const UserScriptList& user_scripts);

    void LoadUserScripts(UserScriptList* user_scripts);

    // Uses extensions_info_ to build a map of localization messages.
    // Returns NULL if |extension_id| is invalid.
    SubstitutionMap* GetLocalizationMessages(std::string extension_id);

    // A pointer back to our master.
    // May be NULL if DisownMaster() is called.
    UserScriptMaster* master_;

    // Maps extension info needed for localization to an extension ID.
    ExtensionsInfo extensions_info_;

    // The message loop to call our master back on.
    // Expected to always outlive us.
    content::BrowserThread::ID master_thread_id_;

    DISALLOW_COPY_AND_ASSIGN(ScriptReloader);
  };

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sends the renderer process a new set of user scripts.
  void SendUpdate(content::RenderProcessHost* process,
                  base::SharedMemory* shared_memory);

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  // We hang on to our pointer to know if we've already got one running.
  scoped_refptr<ScriptReloader> script_reloader_;

  // Contains the scripts that were found the last time scripts were updated.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // List of scripts from currently-installed extensions we should load.
  UserScriptList user_scripts_;

  // Maps extension info needed for localization to an extension ID.
  ExtensionsInfo extensions_info_;

  // If the extensions service has finished loading its initial set of
  // extensions.
  bool extensions_service_ready_;

  // If list of user scripts is modified while we're loading it, we note
  // that we're currently mid-load and then start over again once the load
  // finishes.  This boolean tracks whether another load is pending.
  bool pending_load_;

  // The profile for which the scripts managed here are installed.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptMaster);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
