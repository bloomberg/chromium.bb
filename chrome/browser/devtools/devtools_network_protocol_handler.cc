// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_protocol_handler.h"

#include "base/values.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_controller_handle.h"
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
  int id = 0;
  std::string method;
  base::DictionaryValue* params = nullptr;
  if (!DevToolsProtocol::ParseCommand(command_dict, &id, &method, &params))
    return nullptr;

  namespace network = ::chrome::devtools::Network;

  if (method == network::emulateNetworkConditions::kName)
    return EmulateNetworkConditions(agent_host, id, params).release();

  if (method == network::canEmulateNetworkConditions::kName)
    return CanEmulateNetworkConditions(agent_host, id, params).release();

  return nullptr;
}

scoped_ptr<base::DictionaryValue>
DevToolsNetworkProtocolHandler::CanEmulateNetworkConditions(
    content::DevToolsAgentHost* agent_host,
    int command_id,
    base::DictionaryValue* params) {
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetBoolean(chrome::devtools::kResult, true);
  return DevToolsProtocol::CreateSuccessResponse(command_id, result.Pass());
}

scoped_ptr<base::DictionaryValue>
DevToolsNetworkProtocolHandler::EmulateNetworkConditions(
    content::DevToolsAgentHost* agent_host,
    int command_id,
    base::DictionaryValue* params) {
  namespace names = ::chrome::devtools::Network::emulateNetworkConditions;

  bool offline = false;
  if (!params || !params->GetBoolean(names::kParamOffline, &offline)) {
    return DevToolsProtocol::CreateInvalidParamsResponse(
        command_id, names::kParamOffline);
  }
  double latency = 0.0;
  if (!params->GetDouble(names::kParamLatency, &latency)) {
    return DevToolsProtocol::CreateInvalidParamsResponse(
        command_id, names::kParamLatency);
  }
  if (latency < 0.0)
    latency = 0.0;

  double download_throughput = 0.0;
  if (!params->GetDouble(names::kParamDownloadThroughput,
                         &download_throughput)) {
    return DevToolsProtocol::CreateInvalidParamsResponse(
        command_id, names::kParamDownloadThroughput);
  }
  if (download_throughput < 0.0)
    download_throughput = 0.0;

  double upload_throughput = 0.0;
  if (!params->GetDouble(names::kParamUploadThroughput, &upload_throughput)) {
    return DevToolsProtocol::CreateInvalidParamsResponse(
        command_id, names::kParamUploadThroughput);
  }
  if (upload_throughput < 0.0)
    upload_throughput = 0.0;

  scoped_ptr<DevToolsNetworkConditions> conditions(
      new DevToolsNetworkConditions(
          offline, latency, download_throughput, upload_throughput));

  UpdateNetworkState(agent_host, conditions.Pass());
  return scoped_ptr<base::DictionaryValue>();
}

void DevToolsNetworkProtocolHandler::UpdateNetworkState(
    content::DevToolsAgentHost* agent_host,
    scoped_ptr<DevToolsNetworkConditions> conditions) {
  Profile* profile = Profile::FromBrowserContext(
      agent_host->GetBrowserContext());
  if (!profile)
    return;
  profile->GetDevToolsNetworkControllerHandle()->SetNetworkState(
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
