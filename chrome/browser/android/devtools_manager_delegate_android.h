// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/devtools_manager_delegate.h"

class DevToolsNetworkProtocolHandler;

class DevToolsManagerDelegateAndroid : public content::DevToolsManagerDelegate {
 public:
  DevToolsManagerDelegateAndroid();
  ~DevToolsManagerDelegateAndroid() override;

  // content::DevToolsManagerDelegate implementation.
  void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                 bool attached) override;
  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command_dict) override;
  std::string GetTargetType(content::RenderFrameHost* host) override;
  std::string GetTargetTitle(content::RenderFrameHost* host) override;
  bool DiscoverTargets(
      const content::DevToolsAgentHost::DiscoveryCallback& callback) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url) override;
  std::string GetDiscoveryPageHTML() override;

 private:
  std::unique_ptr<DevToolsNetworkProtocolHandler> network_protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerDelegateAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
