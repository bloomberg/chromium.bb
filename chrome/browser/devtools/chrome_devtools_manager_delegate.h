// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_

#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/base/host_port_pair.h"

class DevToolsNetworkProtocolHandler;

class ChromeDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  static char kTypeApp[];
  static char kTypeBackgroundPage[];
  static char kTypeWebView[];

  ChromeDevToolsManagerDelegate();
  ~ChromeDevToolsManagerDelegate() override;

  // content::DevToolsManagerDelegate implementation.
  void Inspect(content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                 bool attached) override;
  bool DiscoverTargets(
      const content::DevToolsAgentHost::DiscoveryCallback& callback) override;
  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command_dict) override;
  std::string GetTargetType(content::RenderFrameHost* host) override;
  std::string GetTargetTitle(content::RenderFrameHost* host) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url) override;
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;

 private:
  void DevicesAvailable(
    const content::DevToolsAgentHost::DiscoveryCallback& callback,
    const DevToolsAndroidBridge::CompleteDevices& devices);

  std::unique_ptr<base::DictionaryValue> SetRemoteLocations(
      content::DevToolsAgentHost* agent_host,
      int command_id,
      base::DictionaryValue* params);

  std::unique_ptr<DevToolsNetworkProtocolHandler> network_protocol_handler_;
  std::unique_ptr<AndroidDeviceManager> device_manager_;
  std::set<net::HostPortPair> tcp_locations_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsManagerDelegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
