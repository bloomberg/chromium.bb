// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_IMPL_H_
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/browser/debugger/devtools_agent_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"

class GURL;
class RenderViewHost;
class TabContents;

namespace IPC {
class Message;
}

namespace content {

class DevToolsAgentHost;

// This class is a singleton that manages DevToolsClientHost instances and
// routes messages between developer tools clients and agents.
//
// Methods below that accept inspected RenderViewHost as a parameter are
// just convenience methods that call corresponding methods accepting
// DevToolAgentHost.
class CONTENT_EXPORT DevToolsManagerImpl
    : public DevToolsAgentHost::CloseListener,
      public DevToolsManager {
 public:
  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsManagerImpl* GetInstance();

  DevToolsManagerImpl();
  virtual ~DevToolsManagerImpl();

  virtual bool DispatchOnInspectorBackend(DevToolsClientHost* from,
                                          const std::string& message) OVERRIDE;
  void DispatchOnInspectorFrontend(DevToolsAgentHost* agent_host,
                                   const std::string& message);

  void SaveAgentRuntimeState(DevToolsAgentHost* agent_host,
                             const std::string& state);

  // Sends 'Attach' message to the agent using |dest_rvh| in case
  // there is a DevToolsClientHost registered for the |inspected_rvh|.
  void OnNavigatingToPendingEntry(RenderViewHost* inspected_rvh,
                                  RenderViewHost* dest_rvh,
                                  const GURL& gurl);
  void OnCancelPendingNavigation(RenderViewHost* pending,
                                 RenderViewHost* current);

  // Invoked when a tab is replaced by another tab. This is triggered by
  // TabStripModel::ReplaceTabContentsAt.
  virtual void TabReplaced(TabContents* old_tab, TabContents* new_tab) OVERRIDE;

  // Closes all open developer tools windows.
  virtual void CloseAllClientHosts() OVERRIDE;

  virtual void AttachClientHost(int client_host_cookie,
                                DevToolsAgentHost* to_agent) OVERRIDE;
  virtual DevToolsClientHost* GetDevToolsClientHostFor(
      DevToolsAgentHost* agent_host) OVERRIDE;
  virtual void RegisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host,
      DevToolsClientHost* client_host) OVERRIDE;
  virtual void UnregisterDevToolsClientHostFor(
      DevToolsAgentHost* agent_host) OVERRIDE;
  virtual int DetachClientHost(DevToolsAgentHost* from_agent) OVERRIDE;

  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost.
  virtual void ClientHostClosing(DevToolsClientHost* host) OVERRIDE;

  // Starts inspecting element at position (x, y) in the specified page.
  virtual void InspectElement(DevToolsAgentHost* agent_host,
                              int x, int y) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<DevToolsManagerImpl>;

  // DevToolsAgentHost::CloseListener implementation.
  virtual void AgentHostClosing(DevToolsAgentHost* host) OVERRIDE;

  // Returns DevToolsAgentHost inspected by the DevToolsClientHost.
  DevToolsAgentHost* GetAgentHost(DevToolsClientHost* client_host);

  void BindClientHost(DevToolsAgentHost* agent_host,
                      DevToolsClientHost* client_host);
  void UnbindClientHost(DevToolsAgentHost* agent_host,
                        DevToolsClientHost* client_host);

  // Detaches client host and returns cookie that can be used in
  // AttachClientHost.
  int DetachClientHost(RenderViewHost* from_rvh);

  // Attaches orphan client host to new render view host.
  void AttachClientHost(int client_host_cookie,
                        RenderViewHost* to_rvh);

  // These two maps are for tracking dependencies between inspected tabs and
  // their DevToolsClientHosts. They are useful for routing devtools messages
  // and allow us to have at most one devtools client host per tab.
  //
  // DevToolsManagerImpl starts listening to DevToolsClientHosts when they are
  // put into these maps and removes them when they are closing.
  typedef std::map<DevToolsAgentHost*, DevToolsClientHost*>
      AgentToClientHostMap;
  AgentToClientHostMap agent_to_client_host_;

  typedef std::map<DevToolsClientHost*, DevToolsAgentHost*>
      ClientHostToInspectedRvhMap;
  ClientHostToInspectedRvhMap client_to_agent_host_;

  typedef std::map<DevToolsAgentHost*, std::string> AgentRuntimeStates;
  AgentRuntimeStates agent_runtime_states_;

  typedef std::map<int, std::pair<DevToolsClientHost*, std::string> >
      OrphanClientHosts;
  OrphanClientHosts orphan_client_hosts_;
  int last_orphan_cookie_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_IMPL_H_
