// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_PROTOCOL_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/devtools/devtools_protocol.h"

namespace content {
class DevToolsAgentHost;
}

class DevToolsNetworkConditions;

class DevToolsNetworkProtocolHandler {
 public:
  DevToolsNetworkProtocolHandler();
  ~DevToolsNetworkProtocolHandler();

  void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                 bool attached);
  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command_dict);

 private:
  std::unique_ptr<base::DictionaryValue> CanEmulateNetworkConditions(
      content::DevToolsAgentHost* agent_host,
      int command_id,
      base::DictionaryValue* params);

  std::unique_ptr<base::DictionaryValue> EmulateNetworkConditions(
      content::DevToolsAgentHost* agent_host,
      int command_id,
      base::DictionaryValue* params);

  void UpdateNetworkState(
      content::DevToolsAgentHost* agent_host,
      std::unique_ptr<DevToolsNetworkConditions> conditions);

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkProtocolHandler);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_PROTOCOL_HANDLER_H_
