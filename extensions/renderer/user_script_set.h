// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_process_observer.h"
#include "extensions/common/user_script.h"

class GURL;

namespace blink {
class WebFrame;
}

namespace extensions {
class Extension;
class ExtensionSet;
class ScriptInjection;

// The UserScriptSet is a collection of UserScripts which knows how to update
// itself from SharedMemory and create ScriptInjections for UserScripts to
// inject on a page.
class UserScriptSet : public content::RenderProcessObserver {
 public:
  class Observer {
   public:
    virtual void OnUserScriptsUpdated(
        const std::set<std::string>& changed_extensions,
        const std::vector<UserScript*>& scripts) = 0;
  };

  UserScriptSet(const ExtensionSet* extensions);
  virtual ~UserScriptSet();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Appends the ids of the extensions that have user scripts to |ids|.
  void GetActiveExtensionIds(std::set<std::string>* ids) const;

  // Populate |injections| with any ScriptInjections that should run on the
  // given |web_frame| and |tab_id|, at the given |run_location|.
  // |extensions| is passed in to verify the corresponding extension is still
  // valid.
  void GetInjections(ScopedVector<ScriptInjection>* injections,
                     blink::WebFrame* web_frame,
                     int tab_id,
                     UserScript::RunLocation run_location);

 private:
  // content::RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // Handle the UpdateUserScripts extension message.
  void OnUpdateUserScripts(base::SharedMemoryHandle shared_memory,
                           const std::set<std::string>& changed_extensions);

  // Update the parsed scripts from |shared memory|.
  bool UpdateScripts(base::SharedMemoryHandle shared_memory);

  // Returns a new ScriptInjection for the given |script| to execute in the
  // |web_frame|, or NULL if the script should not execute.
  scoped_ptr<ScriptInjection> GetInjectionForScript(
      UserScript* script,
      blink::WebFrame* web_frame,
      int tab_id,
      UserScript::RunLocation run_location,
      const GURL& document_url,
      const Extension* extension);

  // Shared memory containing raw script data.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // The set of all known extensions. Owned by the Dispatcher.
  const ExtensionSet* extensions_;

  // The UserScripts this injector manages.
  ScopedVector<UserScript> scripts_;

  // The associated observers.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
