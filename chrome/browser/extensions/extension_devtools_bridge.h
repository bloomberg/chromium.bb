// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_BRIDGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_BRIDGE_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "content/public/browser/devtools_client_host.h"

class Profile;

// This class is a DevToolsClientHost that fires extension events.
class ExtensionDevToolsBridge : public content::DevToolsClientHost {
 public:
  ExtensionDevToolsBridge(int tab_id, Profile* profile);
  virtual ~ExtensionDevToolsBridge();

  bool RegisterAsDevToolsClientHost();
  void UnregisterAsDevToolsClientHost();

  // DevToolsClientHost, called when the tab inspected by this client is
  // closing.
  virtual void InspectedTabClosing() OVERRIDE;

  // DevToolsClientHost, called to dispatch a message on this client.
  virtual void DispatchOnInspectorFrontend(const std::string& message) OVERRIDE;

  virtual void TabReplaced(TabContents* new_tab) OVERRIDE;

 private:
  virtual void FrameNavigating(const std::string& url) OVERRIDE {}

  // ID of the tab we are monitoring.
  int tab_id_;

  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;

  // Profile that owns our tab
  Profile* profile_;

  // The names of the events fired at extensions depend on the tab id,
  // so we store the various event names in each bridge.
  const std::string on_page_event_name_;
  const std::string on_tab_close_event_name_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsBridge);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_BRIDGE_H_
