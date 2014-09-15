// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_protocol_handler.h"

#include "base/values.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_controller.h"
#include "chrome/browser/devtools/devtools_protocol_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

DevToolsNetworkProtocolHandler::DevToolsNetworkProtocolHandler() {
}

DevToolsNetworkProtocolHandler::~DevToolsNetworkProtocolHandler() {
}

base::DictionaryValue* DevToolsNetworkProtocolHandler::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  scoped_ptr<DevToolsProtocol::Command> command(
       DevToolsProtocol::ParseCommand(command_dict));
  if (!command)
    return NULL;

  namespace network = ::chrome::devtools::Network;
  const std::string method = command->method();
  scoped_ptr<DevToolsProtocol::Response> response;

  if (method == network::emulateNetworkConditions::kName) {
    response = EmulateNetworkConditions(agent_host, command.get());
  } else if (method == network::canEmulateNetworkConditions::kName) {
    response = CanEmulateNetworkConditions(agent_host, command.get());
  }

  if (response)
    return response->Serialize();
  return NULL;
}

Profile* DevToolsNetworkProtocolHandler::GetProfile(
    content::DevToolsAgentHost* agent_host) {
  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents)
    return NULL;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsNetworkProtocolHandler::CanEmulateNetworkConditions(
    content::DevToolsAgentHost* agent_host,
    DevToolsProtocol::Command* command) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetBoolean(chrome::devtools::kResult, true);
  return command->SuccessResponse(result);
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsNetworkProtocolHandler::EmulateNetworkConditions(
    content::DevToolsAgentHost* agent_host,
    DevToolsProtocol::Command* command) {
  base::DictionaryValue* params = command->params();
  namespace names = ::chrome::devtools::Network::emulateNetworkConditions;

  bool offline = false;
  if (!params || !params->GetBoolean(names::kParamOffline, &offline))
    return command->InvalidParamResponse(names::kParamOffline);

  double latency = 0.0;
  if (!params->GetDouble(names::kParamLatency, &latency))
    return command->InvalidParamResponse(names::kParamLatency);
  if (latency < 0.0)
    latency = 0.0;

  double download_throughput = 0.0;
  if (!params->GetDouble(names::kParamDownloadThroughput, &download_throughput))
    return command->InvalidParamResponse(names::kParamDownloadThroughput);
  if (download_throughput < 0.0)
    download_throughput = 0.0;

  double upload_throughput = 0.0;
  if (!params->GetDouble(names::kParamUploadThroughput, &upload_throughput))
    return command->InvalidParamResponse(names::kParamUploadThroughput);
  if (upload_throughput < 0.0)
    upload_throughput = 0.0;

  scoped_ptr<DevToolsNetworkConditions> conditions(
      new DevToolsNetworkConditions(
          offline, latency, download_throughput, upload_throughput));

  UpdateNetworkState(agent_host, conditions.Pass());
  return command->SuccessResponse(NULL);
}

void DevToolsNetworkProtocolHandler::UpdateNetworkState(
    content::DevToolsAgentHost* agent_host,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  Profile* profile = GetProfile(agent_host);
  if (!profile)
    return;
  profile->GetDevToolsNetworkController()->SetNetworkState(
      agent_host->GetId(), conditions.Pass());
}

void DevToolsNetworkProtocolHandler::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  scoped_ptr<DevToolsNetworkConditions> conditions;
  if (attached)
    conditions.reset(new DevToolsNetworkConditions());
  UpdateNetworkState(agent_host, conditions.Pass());
}
