// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include "base/values.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_controller.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_protocol_constants.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate()
    : devtools_callback_(base::Bind(
          &ChromeDevToolsManagerDelegate::OnDevToolsStateChanged,
          base::Unretained(this))),
      devtools_callback_registered_(false) {
  // This constructor is invoked from DevToolsManagerImpl constructor, so it
  // shouldn't call DevToolsManager::GetInstance()
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
  // This destructor is invoked from DevToolsManagerImpl destructor, so it
  // shouldn't call DevToolsManager::GetInstance()
}

void ChromeDevToolsManagerDelegate::EnsureDevtoolsCallbackRegistered() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!devtools_callback_registered_) {
    content::DevToolsManager::GetInstance()->AddAgentStateCallback(
        devtools_callback_);
    devtools_callback_registered_ = true;
  }
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::BrowserContext* browser_context,
    content::DevToolsAgentHost* agent_host) {
  if (!agent_host->IsWorker()) {
    // TODO(horo): Support other types of DevToolsAgentHost when necessary.
    NOTREACHED() << "Inspect() only supports workers.";
  }
#if !defined(OS_ANDROID)
  if (Profile* profile = Profile::FromBrowserContext(browser_context))
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
#endif
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  scoped_ptr<DevToolsProtocol::Command> command(
       DevToolsProtocol::ParseCommand(command_dict));
  if (!command)
    return NULL;
  const std::string method = command->method();
  if (method == chrome::devtools::Network::emulateNetworkConditions::kName)
    return EmulateNetworkConditions(agent_host, command.get())->Serialize();
  return NULL;
}

Profile* ChromeDevToolsManagerDelegate::GetProfile(
    content::DevToolsAgentHost* agent_host) {
  content::RenderViewHost* host = agent_host->GetRenderViewHost();
  if (!host)
    return NULL;
  return Profile::FromBrowserContext(host->GetSiteInstance()->GetProcess()->
      GetBrowserContext());
}

scoped_ptr<DevToolsProtocol::Response>
ChromeDevToolsManagerDelegate::EmulateNetworkConditions(
    content::DevToolsAgentHost* agent_host,
    DevToolsProtocol::Command* command) {
  base::DictionaryValue* params = command->params();

  std::vector<std::string> domains;
  base::ListValue* domain_list = NULL;
  const char* domains_param =
      chrome::devtools::Network::emulateNetworkConditions::kParamDomains;
  if (!params || !params->GetList(domains_param, &domain_list))
    return command->InvalidParamResponse(domains_param);
  size_t size = domain_list->GetSize();
  for (size_t i = 0; i < size; ++i) {
    std::string domain;
    if (!domain_list->GetString(i, &domain))
      return command->InvalidParamResponse(domains_param);
    domains.push_back(domain);
  }

  bool offline = false;
  const char* offline_param =
      chrome::devtools::Network::emulateNetworkConditions::kParamOffline;
  if (!params->GetBoolean(offline_param, &offline))
    return command->InvalidParamResponse(offline_param);

  double maximal_throughput = 0.0;
  const char* maximal_throughput_param =
chrome::devtools::Network::emulateNetworkConditions::kParamMaximalThroughput;
  if (!params->GetDouble(maximal_throughput_param, &maximal_throughput))
    return command->InvalidParamResponse(maximal_throughput_param);
  if (maximal_throughput < 0.0)
    maximal_throughput = 0.0;

  EnsureDevtoolsCallbackRegistered();
  scoped_refptr<DevToolsNetworkConditions> conditions;
  if (offline || maximal_throughput)
    conditions = new DevToolsNetworkConditions(domains, maximal_throughput);
  UpdateNetworkState(agent_host, conditions);
  return command->SuccessResponse(NULL);
}

void ChromeDevToolsManagerDelegate::UpdateNetworkState(
    content::DevToolsAgentHost* agent_host,
    scoped_refptr<DevToolsNetworkConditions> conditions) {
  Profile* profile = GetProfile(agent_host);
  if (!profile)
    return;
  profile->GetDevToolsNetworkController()->SetNetworkState(
      agent_host->GetId(), conditions);
}

void ChromeDevToolsManagerDelegate::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  UpdateNetworkState(agent_host, scoped_refptr<DevToolsNetworkConditions>());
}
