// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace IPC {
class Message;
}

class DevToolsNetLogObserver;
class GURL;
class IOThread;
class PrefService;
class RenderViewHost;
class TabContentsWraper;

using webkit_glue::ResourceLoaderBridge;

typedef std::map<std::string, std::string> DevToolsRuntimeProperties;

// This class is a singleton that manages DevToolsClientHost instances and
// routes messages between developer tools clients and agents.
class DevToolsManager : public DevToolsClientHost::CloseListener,
                        public NotificationObserver,
                        public base::RefCounted<DevToolsManager> {
 public:
  static DevToolsManager* GetInstance();

  static void RegisterUserPrefs(PrefService* prefs);

  DevToolsManager();

  // Returns DevToolsClientHost registered for |inspected_rvh| or NULL if
  // there is no alive DevToolsClientHost registered for |inspected_rvh|.
  DevToolsClientHost* GetDevToolsClientHostFor(RenderViewHost* inspected_rvh);

  // Registers new DevToolsClientHost for |inspected_rvh|. There must be no
  // other DevToolsClientHosts registered for the RenderViewHost at the moment.
  void RegisterDevToolsClientHostFor(RenderViewHost* inspected_rvh,
                                     DevToolsClientHost* client_host);
  void UnregisterDevToolsClientHostFor(RenderViewHost* inspected_rvh);

  void ForwardToDevToolsAgent(RenderViewHost* client_rvh,
                              const IPC::Message& message);
  void ForwardToDevToolsAgent(DevToolsClientHost* from,
                              const IPC::Message& message);
  void ForwardToDevToolsClient(RenderViewHost* inspected_rvh,
                               const IPC::Message& message);

  void ActivateWindow(RenderViewHost* client_rvn);
  void CloseWindow(RenderViewHost* client_rvn);
  void RequestDockWindow(RenderViewHost* client_rvn);
  void RequestUndockWindow(RenderViewHost* client_rvn);

  void OpenDevToolsWindow(RenderViewHost* inspected_rvh);
  void ToggleDevToolsWindow(RenderViewHost* inspected_rvh,
                            DevToolsToggleAction action);
  void RuntimePropertyChanged(RenderViewHost* inspected_rvh,
                              const std::string& name,
                              const std::string& value);

  // Starts element inspection in the devtools client.
  // Creates one by means of OpenDevToolsWindow if no client
  // exists.
  void InspectElement(RenderViewHost* inspected_rvh, int x, int y);

  // Sends 'Attach' message to the agent using |dest_rvh| in case
  // there is a DevToolsClientHost registered for the |inspected_rvh|.
  void OnNavigatingToPendingEntry(RenderViewHost* inspected_rvh,
                                  RenderViewHost* dest_rvh,
                                  const GURL& gurl);

  // Invoked when a tab is replaced by another tab. This is triggered by
  // TabStripModel::ReplaceTabContentsAt.
  void TabReplaced(TabContentsWrapper* old_tab, TabContentsWrapper* new_tab);

  // Detaches client host and returns cookie that can be used in
  // AttachClientHost.
  int DetachClientHost(RenderViewHost* from_rvh);

  // Attaches orphan client host to new render view host.
  void AttachClientHost(int client_host_cookie,
                        RenderViewHost* to_rvh);

  // Closes all open developer tools windows.
  void CloseAllClientHosts();

 private:
  friend class base::RefCounted<DevToolsManager>;

  virtual ~DevToolsManager();

  // DevToolsClientHost::CloseListener override.
  // This method will remove all references from the manager to the
  // DevToolsClientHost and unregister all listeners related to the
  // DevToolsClientHost.
  virtual void ClientHostClosing(DevToolsClientHost* host);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns RenderViewHost for the tab that is inspected by devtools
  // client hosted by DevToolsClientHost.
  RenderViewHost* GetInspectedRenderViewHost(DevToolsClientHost* client_host);

  void SendAttachToAgent(RenderViewHost* inspected_rvh);
  void SendDetachToAgent(RenderViewHost* inspected_rvh);

  void ForceReopenWindow();

  DevToolsClientHost* FindOwnerDevToolsClientHost(RenderViewHost* client_rvh);

  void ToggleDevToolsWindow(RenderViewHost* inspected_rvh,
                            bool force_open,
                            DevToolsToggleAction action);

  void ReopenWindow(RenderViewHost* client_rvh, bool docked);

  void BindClientHost(RenderViewHost* inspected_rvh,
                      DevToolsClientHost* client_host,
                      const DevToolsRuntimeProperties& runtime_properties);

  void UnbindClientHost(RenderViewHost* inspected_rvh,
                        DevToolsClientHost* client_host);

  // These two maps are for tracking dependencies between inspected tabs and
  // their DevToolsClientHosts. They are useful for routing devtools messages
  // and allow us to have at most one devtools client host per tab.
  //
  // DevToolsManager start listening to DevToolsClientHosts when they are put
  // into these maps and removes them when they are closing.
  typedef std::map<RenderViewHost*, DevToolsClientHost*>
      InspectedRvhToClientHostMap;
  InspectedRvhToClientHostMap inspected_rvh_to_client_host_;

  typedef std::map<DevToolsClientHost*, RenderViewHost*>
      ClientHostToInspectedRvhMap;
  ClientHostToInspectedRvhMap client_host_to_inspected_rvh_;

  typedef std::map<RenderViewHost*, DevToolsRuntimeProperties>
      RuntimePropertiesMap;
  RuntimePropertiesMap runtime_properties_map_;

  RenderViewHost* inspected_rvh_for_reopen_;
  bool in_initial_show_;

  typedef std::map<int,
                   std::pair<DevToolsClientHost*, DevToolsRuntimeProperties> >
      OrphanClientHosts;
  OrphanClientHosts orphan_client_hosts_;
  int last_orphan_cookie_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_MANAGER_H_
