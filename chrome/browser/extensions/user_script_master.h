// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/user_script.h"

namespace base {
class SharedMemory;
}

namespace content {
class RenderProcessHost;
}

class Profile;

namespace extensions {

class ContentVerifier;
class ExtensionRegistry;

typedef std::map<std::string, ExtensionSet::ExtensionPathAndDefaultLocale>
    ExtensionsInfo;

// Manages a segment of shared memory that contains the user scripts the user
// has installed.  Lives on the UI thread.
class UserScriptMaster : public content::NotificationObserver,
                         public ExtensionRegistryObserver {
 public:
  // Parses the includes out of |script| and returns them in |includes|.
  static bool ParseMetadataHeader(const base::StringPiece& script_text,
                                  UserScript* script);

  // A wrapper around the method to load user scripts, which is normally run on
  // the file thread. Exposed only for tests.
  static void LoadScriptsForTest(UserScriptList* user_scripts);

  explicit UserScriptMaster(Profile* profile);
  virtual ~UserScriptMaster();

  // Kicks off a process on the file thread to reload scripts from disk
  // into a new chunk of shared memory and notify renderers.
  virtual void StartLoad();

  // Gets the segment of shared memory for the scripts.
  base::SharedMemory* GetSharedMemory() const {
    return shared_memory_.get();
  }

  // Return true if we have any scripts ready.
  bool ScriptsReady() const { return shared_memory_.get() != NULL; }

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // Called once we have finished loading the scripts on the file thread.
  void OnScriptsLoaded(scoped_ptr<UserScriptList> user_scripts,
                       scoped_ptr<base::SharedMemory> shared_memory);

  // Sends the renderer process a new set of user scripts. If
  // |changed_extensions| is not empty, this signals that only the scripts from
  // those extensions should be updated. Otherwise, all extensions will be
  // updated.
  void SendUpdate(content::RenderProcessHost* process,
                  base::SharedMemory* shared_memory,
                  const std::set<std::string>& changed_extensions);

  bool is_loading() const {
    // Ownership of |user_scripts_| is passed to the file thread when loading.
    return user_scripts_.get() == NULL;
  }

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  // Contains the scripts that were found the last time scripts were updated.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // List of scripts from currently-installed extensions we should load.
  scoped_ptr<UserScriptList> user_scripts_;

  // Maps extension info needed for localization to an extension ID.
  ExtensionsInfo extensions_info_;

  // The IDs of the extensions which have changed since the last update sent to
  // the renderer.
  std::set<std::string> changed_extensions_;

  // The mutually-exclusive sets of extensions that were added or removed since
  // the last script load.
  std::set<std::string> added_extensions_;
  std::set<std::string> removed_extensions_;

  // If the extensions service has finished loading its initial set of
  // extensions.
  bool extensions_service_ready_;

  // If list of user scripts is modified while we're loading it, we note
  // that we're currently mid-load and then start over again once the load
  // finishes.  This boolean tracks whether another load is pending.
  bool pending_load_;

  // Whether or not we are currently loading.
  bool is_loading_;

  // The profile for which the scripts managed here are installed.
  Profile* profile_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  base::WeakPtrFactory<UserScriptMaster> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptMaster);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_MASTER_H_
