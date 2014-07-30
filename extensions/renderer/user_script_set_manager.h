// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_process_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/user_script_set.h"

namespace IPC {
class Message;
}

namespace blink {
class WebFrame;
}

namespace extensions {

class ExtensionSet;
class ScriptInjection;

// Manager for separate UserScriptSets, one for each shared memory region.
// Regions are organized as follows:
// static_scripts -- contains all extensions' scripts that are statically
//                   declared in the extension manifest.
// programmatic_scripts -- one region per extension containing only
//                         programmatically-declared scripts, instantiated
//                         when an extension first creates a declarative rule
//                         that would, if triggered, request a script injection.
class UserScriptSetManager : public content::RenderProcessObserver {
 public:
  // Like a UserScriptSet::Observer, but automatically subscribes to all sets
  // associated with the manager.
  class Observer {
   public:
    virtual void OnUserScriptsUpdated(
        const std::set<std::string>& changed_extensions,
        const std::vector<UserScript*>& scripts) = 0;
  };

  UserScriptSetManager(const ExtensionSet* extensions);

  virtual ~UserScriptSetManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const UserScriptSet* GetProgrammaticScriptsByExtension(
      const ExtensionId& extensionId);

  // Put all injections from |static_scripts| and each of
  // |programmatic_scripts_| into |injections|.
  void GetAllInjections(ScopedVector<ScriptInjection>* injections,
                        blink::WebFrame* web_frame,
                        int tab_id,
                        UserScript::RunLocation run_location);

  // Get active extension IDs from |static_scripts| and each of
  // |programmatic_scripts_|.
  void GetAllActiveExtensionIds(std::set<std::string>* ids) const;

  const UserScriptSet* static_scripts() const { return &static_scripts_; }

 private:
  // Map for per-extension sets that may be defined programmatically.
  typedef std::map<ExtensionId, linked_ptr<UserScriptSet> > UserScriptSetMap;

  // content::RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // Handle the UpdateUserScripts extension message.
  void OnUpdateUserScripts(base::SharedMemoryHandle shared_memory,
                           const ExtensionId& extension_id,
                           const std::set<std::string>& changed_extensions);

  // Scripts statically defined in extension manifests.
  UserScriptSet static_scripts_;

  // Scripts programmatically-defined through API calls (initialized and stored
  // per-extension).
  UserScriptSetMap programmatic_scripts_;

  // The set of all known extensions. Owned by the Dispatcher.
  const ExtensionSet* extensions_;

  // The associated observers.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSetManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_
