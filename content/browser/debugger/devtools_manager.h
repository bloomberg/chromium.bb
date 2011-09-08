// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#pragma once

#include <map>
#include <string>

#include "content/browser/debugger/devtools_agent_host.h"
#include "content/browser/debugger/devtools_client_host.h"

namespace IPC {
class Message;
}

class DevToolsAgentHost;
class DevToolsNetLogObserver;
class GURL;
class IOThread;
class PrefService;
class RenderViewHost;
class TabContents;

typedef std::map<std::string, std::string> DevToolsRuntimeProperties;

// This class is a singleton that manages DevToolsClientHost instances and
// routes messages between developer tools clients and agents.
//
// Methods below that accept inspected RenderViewHost as a parameter are
// just convenience methods that call corresponding methods accepting
// DevToolAgentHost.
class DevToolsManager : public DevToolsClientHost::CloseListener,
                        public DevToolsAgentHost::CloseListener {
 public:
  static DevToolsManager* GetInstance();

  DevToolsManager();
  virtual ~DevToolsManager();

  // Returns DevToolsClientHost registered for |inspected_rvh| or NULL if
  // there is no alive DevToolsClientHost registered for |inspected_rvh|.
  DevToolsClientHost* GetDevToolsClientHostFor(RenderViewHost* inspected_rvh);

  // Registers new DevToolsClientHost for |inspected_rvh|. There must be no
  // other DevToolsClientHosts registered for the RenderViewHost at the moment.
  void RegisterDevToolsClientHostFor(RenderViewHost* inspected_rvh,
                                     DevToolsClientHost* client_host);
  void UnregisterDevToolsClientHostFor(RenderViewHost* inspected_rvh);

  bool ForwardToDevToolsAgent(DevToolsClientHost* from,
                              const IPC::Message& message);
  void ForwardToDevToolsClient(DevToolsAgentHost* agent_host,
                               const IPC::Message& message);

  void RuntimePropertyChanged(DevToolsAgentHost* agent_host,
                              const std::string& name,
                              const std::string& value);

  // Starts element inspection in the devtools agent.
  void SendInspectElement(RenderViewHost* inspected_rvh, int x, int y);

  // Sends 'Attach' message to the agent using |dest_rvh| in case
  // there is a DevToolsClientHost registered for the |inspected_rvh|.
  void OnNavigatingToPendingEntry(RenderViewHost* inspected_rvh,
                                  RenderViewHost* dest_rvh,
                                  const GURL& gurl);

  // Invoked when a tab is replaced by another tab. This is triggered by
  // TabStripModel::ReplaceTabContentsAt.
  void TabReplaced(TabContents* old_tab, TabContents* new_tab);

  // Detaches client host and returns cookie that can be used in
  // AttachClientHost.
  int DetachClientHost(RenderViewHost* from_rvh);

  // Attaches orphan client host to new render view host.
  void AttachClientHost(int client_host_cookie,
                        RenderViewHost* to_rvh);

  // Closes all open developer tools windows.
  void CloseAllClientHosts();

  DevToolsClientHost* GetDevToolsClientHostFor(DevToolsAgentHost* agent_host);
  void RegisterDevToolsClientHostFor(DevToolsAgentHost* agent_host,
                                     DevToolsClientHost* client_host);
  void UnregisterDevToolsClientHostFor(DevToolsAgentHost* agent_host);

 private:
  // DevToolsClientHost::CloseListener override.
  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost.
  virtual void ClientHostClosing(DevToolsClientHost* host);

  // DevToolsAgentHost::CloseListener implementation.
  virtual void AgentHostClosing(DevToolsAgentHost* host);

  // Returns DevToolsAgentHost inspected by the DevToolsClientHost.
  DevToolsAgentHost* GetAgentHost(DevToolsClientHost* client_host);

  void SendAttachToAgent(DevToolsAgentHost*);
  void SendDetachToAgent(DevToolsAgentHost*);

  void BindClientHost(DevToolsAgentHost* agent_host,
                      DevToolsClientHost* client_host,
                      const DevToolsRuntimeProperties& runtime_properties);
  void UnbindClientHost(DevToolsAgentHost* agent_host,
                        DevToolsClientHost* client_host);

  // These two maps are for tracking dependencies between inspected tabs and
  // their DevToolsClientHosts. They are useful for routing devtools messages
  // and allow us to have at most one devtools client host per tab.
  //
  // DevToolsManager start listening to DevToolsClientHosts when they are put
  // into these maps and removes them when they are closing.
  typedef std::map<DevToolsAgentHost*, DevToolsClientHost*>
      AgentToClientHostMap;
  AgentToClientHostMap agent_to_client_host_;

  typedef std::map<DevToolsClientHost*, DevToolsAgentHost*>
      ClientHostToInspectedRvhMap;
  ClientHostToInspectedRvhMap client_to_agent_host_;

  typedef std::map<DevToolsAgentHost*, DevToolsRuntimeProperties>
      RuntimePropertiesMap;
  RuntimePropertiesMap runtime_properties_map_;

  typedef std::map<int,
                   std::pair<DevToolsClientHost*, DevToolsRuntimeProperties> >
      OrphanClientHosts;
  OrphanClientHosts orphan_client_hosts_;
  int last_orphan_cookie_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
