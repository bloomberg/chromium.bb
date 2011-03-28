// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"

class ExtensionDevToolsBridge;
class MessageLoop;
class Profile;

// This class manages the lifetimes of ExtensionDevToolsBridge objects.
// The manager is owned by the Profile.
//
// The lifetime of an ExtensionDevToolsBridge object is determined by:
//  * the existence of registered event handlers for the bridge's tab
//  * the lifetime of the inspected tab
//
// The manager is alerted whenever an event listener is added or removed and
// keeps track of the set of renderers with event listeners registered for each
// tab.  A new bridge object is created for a tab when the first event listener
// is registered on that tab.  A bridge object is destroyed when all event
// listeners are removed, the inspected tab closes, or when the manager itself
// is destroyed.

class ExtensionDevToolsManager
    : public base::RefCountedThreadSafe<ExtensionDevToolsManager> {
 public:
  // UI thread only:
  explicit ExtensionDevToolsManager(Profile* profile);

  void AddEventListener(const std::string& event_name,
                        int render_process_id);

  void RemoveEventListener(const std::string& event_name,
                           int render_process_id);

  void BridgeClosingForTab(int tab_id);

 private:
  friend class base::RefCountedThreadSafe<ExtensionDevToolsManager>;

  ~ExtensionDevToolsManager();

  // Map of tab IDs to the ExtensionDevToolsBridge connected to the tab
  std::map<int, linked_ptr<ExtensionDevToolsBridge> > tab_id_to_bridge_;

  // Map of tab IDs to the set of render_process_ids that have registered
  // event handlers for the tab.
  std::map<int, std::set<int> > tab_id_to_render_process_ids_;

  Profile* profile_;
  MessageLoop* ui_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_MANAGER_H_

