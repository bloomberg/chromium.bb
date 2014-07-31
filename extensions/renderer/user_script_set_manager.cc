// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set_manager.h"

#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/user_script_set.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace extensions {

UserScriptSetManager::UserScriptSetManager(const ExtensionSet* extensions)
    : static_scripts_(extensions), extensions_(extensions) {
  content::RenderThread::Get()->AddObserver(this);
}

UserScriptSetManager::~UserScriptSetManager() {
}

void UserScriptSetManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSetManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool UserScriptSetManager::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UserScriptSetManager, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

const UserScriptSet* UserScriptSetManager::GetProgrammaticScriptsByExtension(
    const ExtensionId& extension_id) {
  UserScriptSetMap::const_iterator it =
      programmatic_scripts_.find(extension_id);
  return it != programmatic_scripts_.end() ? it->second.get() : NULL;
}

void UserScriptSetManager::GetAllInjections(
    ScopedVector<ScriptInjection>* injections,
    blink::WebFrame* web_frame,
    int tab_id,
    UserScript::RunLocation run_location) {
  static_scripts_.GetInjections(injections, web_frame, tab_id, run_location);
  for (UserScriptSetMap::iterator it = programmatic_scripts_.begin();
       it != programmatic_scripts_.end();
       ++it) {
    it->second->GetInjections(injections, web_frame, tab_id, run_location);
  }
}

void UserScriptSetManager::GetAllActiveExtensionIds(
    std::set<std::string>* ids) const {
  DCHECK(ids);
  static_scripts_.GetActiveExtensionIds(ids);
  for (UserScriptSetMap::const_iterator it = programmatic_scripts_.begin();
       it != programmatic_scripts_.end();
       ++it) {
    it->second->GetActiveExtensionIds(ids);
  }
}

void UserScriptSetManager::OnUpdateUserScripts(
    base::SharedMemoryHandle shared_memory,
    const ExtensionId& extension_id,
    const std::set<std::string>& changed_extensions) {
  if (!base::SharedMemory::IsHandleValid(shared_memory)) {
    NOTREACHED() << "Bad scripts handle";
    return;
  }

  for (std::set<std::string>::const_iterator iter = changed_extensions.begin();
       iter != changed_extensions.end();
       ++iter) {
    if (!Extension::IdIsValid(*iter)) {
      NOTREACHED() << "Invalid extension id: " << *iter;
      return;
    }
  }

  UserScriptSet* scripts = NULL;
  if (!extension_id.empty()) {
    // The expectation when there is an extensions that "owns" this shared
    // memory region is that it will list itself as the only changed extension.
    CHECK(changed_extensions.size() == 1 &&
          changed_extensions.find(extension_id) != changed_extensions.end());
    if (programmatic_scripts_.find(extension_id) ==
        programmatic_scripts_.end()) {
      scripts = new UserScriptSet(extensions_);
      programmatic_scripts_[extension_id] = make_linked_ptr(scripts);
    } else {
      scripts = programmatic_scripts_[extension_id].get();
    }
  } else {
    scripts = &static_scripts_;
  }
  DCHECK(scripts);

  // If no extensions are included in the set, that indicates that all
  // extensions were updated. Add them all to the set so that observers and
  // individual UserScriptSets don't need to know this detail.
  const std::set<std::string>* effective_extensions = &changed_extensions;
  std::set<std::string> all_extensions;
  if (changed_extensions.empty()) {
    all_extensions = extensions_->GetIDs();
    effective_extensions = &all_extensions;
  }

  if (scripts->UpdateUserScripts(shared_memory, *effective_extensions)) {
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        OnUserScriptsUpdated(*effective_extensions, scripts->scripts()));
  }
}

}  // namespace extensions
