// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_BRIDGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_BRIDGE_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"

class Profile;

// This class is a DevToolsClientHost that fires extension events.
class ExtensionDevToolsBridge : public DevToolsClientHost {
 public:
  ExtensionDevToolsBridge(int tab_id, Profile* profile);
  virtual ~ExtensionDevToolsBridge();

  bool RegisterAsDevToolsClientHost();
  void UnregisterAsDevToolsClientHost();

  // DevToolsClientHost, called when the tab inspected by this client is
  // closing.
  virtual void InspectedTabClosing();

  // DevToolsClientHost, called to send a message to this host.
  virtual void SendMessageToClient(const IPC::Message& msg);

  virtual void TabReplaced(TabContentsWrapper* new_tab);

 private:
  void OnDispatchOnInspectorFrontend(const std::string& data);

  virtual void FrameNavigating(const std::string& url) {}

  // ID of the tab we are monitoring.
  int tab_id_;

  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;
  scoped_refptr<ExtensionMessageService> extension_message_service_;

  // Profile that owns our tab
  Profile* profile_;

  // The names of the events fired at extensions depend on the tab id,
  // so we store the various event names in each bridge.
  const std::string on_page_event_name_;
  const std::string on_tab_close_event_name_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsBridge);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_BRIDGE_H_

