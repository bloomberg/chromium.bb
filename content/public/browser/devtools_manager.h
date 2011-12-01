// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
#pragma once

#include <string>

#include "content/common/content_export.h"

class TabContents;

namespace IPC {
class Message;
}

namespace content {

class DevToolsAgentHost;
class DevToolsClientHost;

// DevToolsManager connects a devtools client to inspected agent and routes
// devtools messages between the inspected instance represented by the agent
// and devtools front-end represented by the client. If inspected agent
// gets terminated DevToolsManager will notify corresponding client and
// remove it from the map.
class CONTENT_EXPORT DevToolsManager {
 public:
  static DevToolsManager* GetInstance();

  // Routes devtools message from |from| client to corresponding
  // DevToolsAgentHost.
  virtual bool DispatchOnInspectorBackend(DevToolsClientHost* from,
                                          const std::string& message) = 0;

  // Invoked when a tab is replaced by another tab. This is triggered by
  // TabStripModel::ReplaceTabContentsAt.
  virtual void TabReplaced(TabContents* old_tab, TabContents* new_tab) = 0;

  // Closes all open developer tools windows.
  virtual void CloseAllClientHosts() = 0;

  // Returns client attached to the |agent_host| if there is one.
  virtual DevToolsClientHost* GetDevToolsClientHostFor(
      DevToolsAgentHost* agent_host) = 0;

  // Registers new DevToolsClientHost for inspected |agent_host|. There must be
  // no other DevToolsClientHosts registered for the |agent_host| at the moment.
  virtual void RegisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host,
      DevToolsClientHost* client_host) = 0;
  // Unregisters given |agent_host|. DevToolsManager will notify corresponding
  // client if one is attached.
  virtual void UnregisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host) = 0;

  // Detaches client from |from_agent| and returns a cookie that allows to
  // reattach that client to another agent later. Returns -1 if there is no
  // client attached to |from_agent|.
  virtual int DetachClientHost(DevToolsAgentHost* from_agent) = 0;
  // Reattaches client host detached with DetachClientHost method above
  // to |to_agent|.
  virtual void AttachClientHost(int client_host_cookie,
                                DevToolsAgentHost* to_agent) = 0;

  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost. Called by closing client.
  virtual void ClientHostClosing(DevToolsClientHost* client_host) = 0;

  // Starts inspecting element at position (x, y) in the specified page.
  virtual void InspectElement(DevToolsAgentHost* agent_host, int x, int y) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
