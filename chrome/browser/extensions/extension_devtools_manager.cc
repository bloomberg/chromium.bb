// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_devtools_manager.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_devtools_bridge.h"
#include "chrome/browser/extensions/extension_devtools_events.h"

ExtensionDevToolsManager::ExtensionDevToolsManager(Profile* profile)
    : profile_(profile),
      ui_loop_(NULL) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  ui_loop_ = MessageLoop::current();
}

ExtensionDevToolsManager::~ExtensionDevToolsManager() {
}

void ExtensionDevToolsManager::AddEventListener(const std::string& event_name,
                                                int render_process_id) {
  int tab_id;
  if (ExtensionDevToolsEvents::IsDevToolsEventName(event_name, &tab_id)) {
    // Add the renderer process ID to the set of processes interested
    // in this tab.
    tab_id_to_render_process_ids_[tab_id].insert(render_process_id);
    if (tab_id_to_bridge_.count(tab_id) == 0) {
      // Create a new bridge for this tab if there isn't one already.
      linked_ptr<ExtensionDevToolsBridge> bridge(
          new ExtensionDevToolsBridge(tab_id, profile_));
      if (bridge->RegisterAsDevToolsClientHost()) {
        tab_id_to_bridge_[tab_id] = bridge;
      }
    }
  }
}

void ExtensionDevToolsManager::RemoveEventListener(
    const std::string& event_name,
    int render_process_id) {
  int tab_id;
  if (ExtensionDevToolsEvents::IsDevToolsEventName(event_name, &tab_id)) {
    std::map<int, std::set<int> >::iterator it =
        tab_id_to_render_process_ids_.find(tab_id);
    if (it != tab_id_to_render_process_ids_.end()) {
      // Remove the process from the set of processes interested in this tab.
      it->second.erase(render_process_id);
      if (it->second.empty()) {
        // No renderers have registered listeners for this tab, so kill the
        // bridge if there is one.
        if (tab_id_to_bridge_.count(tab_id) != 0) {
          linked_ptr<ExtensionDevToolsBridge> bridge(tab_id_to_bridge_[tab_id]);
          bridge->UnregisterAsDevToolsClientHost();
          tab_id_to_bridge_.erase(tab_id);
        }
      }
    }
  }
}

void ExtensionDevToolsManager::BridgeClosingForTab(int tab_id) {
  if (tab_id_to_bridge_.count(tab_id) != 0) {
    linked_ptr<ExtensionDevToolsBridge> bridge(tab_id_to_bridge_[tab_id]);
    bridge->UnregisterAsDevToolsClientHost();
    tab_id_to_bridge_.erase(tab_id);
  }
  tab_id_to_render_process_ids_.erase(tab_id);
}

