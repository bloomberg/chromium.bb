// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/devtools/device/devtools_device_discovery.h"
#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/base/host_port_pair.h"

class ChromeDevToolsSession;

class ChromeDevToolsManagerDelegate :
    public content::DevToolsManagerDelegate,
    public content::DevToolsAgentHostObserver {
 public:
  static char kTypeApp[];
  static char kTypeBackgroundPage[];

  ChromeDevToolsManagerDelegate();
  ~ChromeDevToolsManagerDelegate() override;

 private:
  class HostData;
  friend class DevToolsManagerDelegateTest;
  using RemoteLocations = std::set<net::HostPortPair>;

  // content::DevToolsManagerDelegate implementation.
  void Inspect(content::DevToolsAgentHost* agent_host) override;
  bool HandleCommand(content::DevToolsAgentHost* agent_host,
                     int session_id,
                     base::DictionaryValue* command_dict) override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  std::string GetTargetTitle(content::WebContents* web_contents) override;
  void SessionCreated(content::DevToolsAgentHost* agent_host,
                      int session_id) override;
  void SessionDestroyed(content::DevToolsAgentHost* agent_host,
                        int session_id) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url) override;
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;

  // content::DevToolsAgentHostObserver overrides.
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

  void UpdateDeviceDiscovery();
  void DevicesAvailable(
      const DevToolsDeviceDiscovery::CompleteDevices& devices);

  std::unique_ptr<base::DictionaryValue> SetRemoteLocations(
      content::DevToolsAgentHost* agent_host,
      int command_id,
      base::DictionaryValue* params);

  std::map<content::DevToolsAgentHost*, std::unique_ptr<HostData>> host_data_;

  std::map<int, std::unique_ptr<ChromeDevToolsSession>> sessions_;

  std::unique_ptr<AndroidDeviceManager> device_manager_;
  std::unique_ptr<DevToolsDeviceDiscovery> device_discovery_;
  content::DevToolsAgentHost::List remote_agent_hosts_;
  RemoteLocations remote_locations_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsManagerDelegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
