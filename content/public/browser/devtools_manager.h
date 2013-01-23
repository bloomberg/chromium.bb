// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/console_message_level.h"

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

  virtual ~DevToolsManager() {}

  // Routes devtools message from |from| client to corresponding
  // DevToolsAgentHost.
  virtual bool DispatchOnInspectorBackend(DevToolsClientHost* from,
                                          const std::string& message) = 0;

  // Closes all open developer tools windows.
  virtual void CloseAllClientHosts() = 0;

  // Returns client attached to the |agent_host| if there is one.
  virtual DevToolsClientHost* GetDevToolsClientHostFor(
      DevToolsAgentHost* agent_host) = 0;

  // Returns agent that has |client_host| attachd to it if there is one.
  virtual DevToolsAgentHost* GetDevToolsAgentHostFor(
      DevToolsClientHost* client_host) = 0;

  // Registers new DevToolsClientHost for inspected |agent_host|. There must be
  // no other DevToolsClientHosts registered for the |agent_host| at the moment.
  virtual void RegisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host,
      DevToolsClientHost* client_host) = 0;
  // Unregisters given |agent_host|. DevToolsManager will notify corresponding
  // client if one is attached.
  virtual void UnregisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host) = 0;

  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost. Called by closing client.
  virtual void ClientHostClosing(DevToolsClientHost* client_host) = 0;

  // Starts inspecting element at position (x, y) in the specified page.
  virtual void InspectElement(DevToolsAgentHost* agent_host, int x, int y) = 0;

  // Logs given |message| on behalf of the given |agent_host|.
  virtual void AddMessageToConsole(DevToolsAgentHost* agent_host,
                                   ConsoleMessageLevel level,
                                   const std::string& message) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_H_
