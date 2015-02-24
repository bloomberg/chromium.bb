// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_USER_SCRIPT_LOADER_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/host_id.h"
#include "extensions/common/user_script.h"

namespace base {
class SharedMemory;
}

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace extensions {

class ContentVerifier;

// Manages one "logical unit" of user scripts in shared memory by constructing a
// new shared memory region when the set of scripts changes. Also notifies
// renderers of new shared memory region when new renderers appear, or when
// script reloading completes. Script loading lives on the UI thread. Instances
// of this class are embedded within classes with names ending in
// UserScriptMaster. These "master" classes implement the strategy for which
// scripts to load/unload on this logical unit of scripts.
class UserScriptLoader : public content::NotificationObserver {
 public:
  using PathAndDefaultLocale = std::pair<base::FilePath, std::string>;
  using HostsInfo = std::map<HostID, PathAndDefaultLocale>;

  using SubstitutionMap = std::map<std::string, std::string>;
  using LoadUserScriptsContentFunction =
      base::Callback<bool(const HostID&,
                          UserScript::File*,
                          const SubstitutionMap*,
                          const scoped_refptr<ContentVerifier>&)>;

  // Parses the includes out of |script| and returns them in |includes|.
  static bool ParseMetadataHeader(const base::StringPiece& script_text,
                                  UserScript* script);

  UserScriptLoader(content::BrowserContext* browser_context,
                   const HostID& host_id,
                   const scoped_refptr<ContentVerifier>& content_verifier);
  ~UserScriptLoader() override;

  // A wrapper around the method to load user scripts, which is normally run on
  // the file thread. Exposed only for tests.
  void LoadScriptsForTest(UserScriptList* user_scripts);

  // Add |scripts| to the set of scripts managed by this loader.
  void AddScripts(const std::set<UserScript>& scripts);

  // Remove |scripts| from the set of scripts managed by this loader.
  void RemoveScripts(const std::set<UserScript>& scripts);

  // Clears the set of scripts managed by this loader.
  void ClearScripts();

  // Initiates procedure to start loading scripts on the file thread.
  void StartLoad();

  // Returns true if we have any scripts ready.
  bool scripts_ready() const { return shared_memory_.get() != NULL; }

 protected:
  // Updates |hosts_info_| to contain info for each element of
  // |changed_hosts_|.
  virtual void UpdateHostsInfo(const std::set<HostID>& changed_hosts) = 0;

  // Returns a function pointer of a static funcion to load user scripts.
  // Derived classes can specify their ways to load scripts in the static
  // function they return.
  // Note: It has to be safe to call multiple times.
  virtual LoadUserScriptsContentFunction GetLoadUserScriptsFunction() = 0;

  // Adds the |host_id, location| to the |hosts_info_| map.
  // Only inserts the entry to the map when the given host_id doesn't
  // exists.
  void AddHostInfo(const HostID& host_id, const PathAndDefaultLocale& location);

  // Removes the entries with the given host_id from the |hosts_info_| map.
  void RemoveHostInfo(const HostID& host_id);

  // Sets the flag if the initial set of hosts has finished loading; if it's
  // set to be true, calls AttempLoad() to bootstrap.
  void SetReady(bool ready);

  content::BrowserContext* browser_context() const { return browser_context_; }
  const HostID& host_id() const { return host_id_; }

 private:
  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns whether or not it is possible that calls to AddScripts(),
  // RemoveScripts(), and/or ClearScripts() have caused any real change in the
  // set of scripts to be loaded.
  bool ScriptsMayHaveChanged() const;

  // Attempts to initiate a load.
  void AttemptLoad();

  // Called once we have finished loading the scripts on the file thread.
  void OnScriptsLoaded(scoped_ptr<UserScriptList> user_scripts,
                       scoped_ptr<base::SharedMemory> shared_memory);

  // Sends the renderer process a new set of user scripts. If
  // |changed_hosts| is not empty, this signals that only the scripts from
  // those hosts should be updated. Otherwise, all hosts will be
  // updated.
  void SendUpdate(content::RenderProcessHost* process,
                  base::SharedMemory* shared_memory,
                  const std::set<HostID>& changed_hosts);

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

  // Maps host info needed for localization to a host ID.
  HostsInfo hosts_info_;

  // The mutually-exclusive sets of scripts that were added or removed since the
  // last script load.
  std::set<UserScript> added_scripts_;
  std::set<UserScript> removed_scripts_;

  // Indicates whether the the collection of scripts should be cleared before
  // additions and removals on the next script load.
  bool clear_scripts_;

  // The IDs of the extensions which changed in the last update sent to the
  // renderer.
  std::set<HostID> changed_hosts_;

  // If the initial set of hosts has finished loading.
  bool ready_;

  // If list of user scripts is modified while we're loading it, we note
  // that we're currently mid-load and then start over again once the load
  // finishes.  This boolean tracks whether another load is pending.
  bool pending_load_;

  // Whether or not we are currently loading.
  bool is_loading_;

  // The browser_context for which the scripts managed here are installed.
  content::BrowserContext* browser_context_;

  // ID of the host that owns these scripts, if any. This is only set to a
  // non-empty value for declarative user script shared memory regions.
  HostID host_id_;

  // Manages content verification of the loaded user scripts.
  scoped_refptr<ContentVerifier> content_verifier_;

  base::WeakPtrFactory<UserScriptLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_USER_SCRIPT_LOADER_H_
