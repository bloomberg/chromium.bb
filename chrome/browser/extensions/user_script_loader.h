// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LOADER_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/user_script.h"

namespace base {
class SharedMemory;
}

namespace content {
class BrowserContext;
class RenderProcessHost;
}

class Profile;

namespace extensions {

class ContentVerifier;
class ExtensionRegistry;

typedef std::map<ExtensionId, ExtensionSet::ExtensionPathAndDefaultLocale>
    ExtensionsInfo;

// Manages one "logical unit" of user scripts in shared memory by constructing a
// new shared memory region when the set of scripts changes. Also notifies
// renderers of new shared memory region when new renderers appear, or when
// script reloading completes. Script loading lives on the UI thread. Instances
// of this class are embedded within classes with names ending in
// UserScriptMaster. These "master" classes implement the strategy for which
// scripts to load/unload on this logical unit of scripts.
class UserScriptLoader : public content::NotificationObserver,
                         public ExtensionRegistryObserver {
 public:
  // Parses the includes out of |script| and returns them in |includes|.
  static bool ParseMetadataHeader(const base::StringPiece& script_text,
                                  UserScript* script);

  // A wrapper around the method to load user scripts, which is normally run on
  // the file thread. Exposed only for tests.
  static void LoadScriptsForTest(UserScriptList* user_scripts);

  UserScriptLoader(Profile* profile,
                   const ExtensionId& owner_extension_id,
                   bool listen_for_extension_system_loaded);
  virtual ~UserScriptLoader();

  // Add |scripts| to the set of scripts managed by this loader.
  void AddScripts(const std::set<UserScript>& scripts);

  // Remove |scripts| from the set of scripts managed by this loader.
  void RemoveScripts(const std::set<UserScript>& scripts);

  // Clears the set of scripts managed by this loader.
  void ClearScripts();

  // Initiates procedure to start loading scripts on the file thread.
  void StartLoad();

  // Return true if we have any scripts ready.
  bool scripts_ready() const { return shared_memory_.get() != NULL; }

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // Initiates script load when we have been waiting for the extension system
  // to be ready.
  void OnExtensionSystemReady();

  // Returns whether or not it is possible that calls to AddScripts(),
  // RemoveScripts(), and/or ClearScripts() have caused any real change in the
  // set of scripts to be loaded.
  bool ScriptsMayHaveChanged() const;

  // Attempt to initiate a load.
  void AttemptLoad();

  // Called once we have finished loading the scripts on the file thread.
  void OnScriptsLoaded(scoped_ptr<UserScriptList> user_scripts,
                       scoped_ptr<base::SharedMemory> shared_memory);

  // Sends the renderer process a new set of user scripts. If
  // |changed_extensions| is not empty, this signals that only the scripts from
  // those extensions should be updated. Otherwise, all extensions will be
  // updated.
  void SendUpdate(content::RenderProcessHost* process,
                  base::SharedMemory* shared_memory,
                  const std::set<ExtensionId>& changed_extensions);

  // Add to |changed_extensions_| those extensions referred to by |scripts|.
  void ExpandChangedExtensions(const std::set<UserScript>& scripts);

  // Update |extensions_info_| to contain info for each element of
  // |changed_extensions_|.
  void UpdateExtensionsInfo();

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

  // The mutually-exclusive sets of scripts that were added or removed since the
  // last script load.
  std::set<UserScript> added_scripts_;
  std::set<UserScript> removed_scripts_;

  // Indicates whether the the collection of scripts should be cleared before
  // additions and removals on the next script load.
  bool clear_scripts_;

  // The IDs of the extensions which changed in the last update sent to the
  // renderer.
  ExtensionIdSet changed_extensions_;

  // If the extensions service has finished loading its initial set of
  // extensions.
  bool extension_system_ready_;

  // If list of user scripts is modified while we're loading it, we note
  // that we're currently mid-load and then start over again once the load
  // finishes.  This boolean tracks whether another load is pending.
  bool pending_load_;

  // Whether or not we are currently loading.
  bool is_loading_;

  // The profile for which the scripts managed here are installed.
  Profile* profile_;

  // ID of the extension that owns these scripts, if any. This is only set to a
  // non-empty value for declarative user script shared memory regions.
  ExtensionId owner_extension_id_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  base::WeakPtrFactory<UserScriptLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LOADER_H_
