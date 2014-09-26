// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> Instances;
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

typedef std::vector<const DevToolsAgentHost::AgentStateCallback*>
    AgentStateCallbacks;
base::LazyInstance<AgentStateCallbacks>::Leaky g_callbacks =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  List result = EmbeddedWorkerDevToolsManager::GetInstance()
      ->GetOrCreateAllAgentHosts();
  std::vector<WebContents*> wc_list =
      DevToolsAgentHostImpl::GetInspectableWebContents();
  for (std::vector<WebContents*>::iterator it = wc_list.begin();
      it != wc_list.end(); ++it) {
    result.push_back(GetOrCreateFor(*it));
  }
  return result;
}

DevToolsAgentHostImpl::DevToolsAgentHostImpl()
    : id_(base::GenerateGUID()),
      client_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_instances.Get()[id_] = this;
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_instances.Get().erase(g_instances.Get().find(id_));
}

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  if (g_instances == NULL)
    return NULL;
  Instances::iterator it = g_instances.Get().find(id);
  if (it == g_instances.Get().end())
    return NULL;
  return it->second;
}

//static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Create(
    DevToolsExternalAgentProxyDelegate* delegate) {
  return new ForwardingAgentHost(delegate);
}

void DevToolsAgentHostImpl::AttachClient(DevToolsAgentHostClient* client) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  if (client_) {
    client_->AgentHostClosed(this, true);
    Detach();
  } else {
    DevToolsManager::GetInstance()->OnClientAttached();
  }
  client_ = client;
  Attach();
}

void DevToolsAgentHostImpl::DetachClient() {
  if (!client_)
    return;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  client_ = NULL;
  DevToolsManager::GetInstance()->OnClientDetached();
  Detach();
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !!client_;
}

void DevToolsAgentHostImpl::InspectElement(int x, int y) {
}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

WebContents* DevToolsAgentHostImpl::GetWebContents() {
  return NULL;
}

void DevToolsAgentHostImpl::DisconnectWebContents() {
}

void DevToolsAgentHostImpl::ConnectWebContents(WebContents* wc) {
}

bool DevToolsAgentHostImpl::IsWorker() const {
  return false;
}

void DevToolsAgentHostImpl::HostClosed() {
  if (!client_)
    return;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  // Clear |client_| before notifying it.
  DevToolsAgentHostClient* client = client_;
  client_ = NULL;
  DevToolsManager::GetInstance()->OnClientDetached();
  client->AgentHostClosed(this, false);
}

void DevToolsAgentHostImpl::SendMessageToClient(const std::string& message) {
  if (!client_)
    return;
  client_->DispatchProtocolMessage(this, message);
}

// static
void DevToolsAgentHost::DetachAllClients() {
  if (g_instances == NULL)
    return;

  // Make a copy, since detaching may lead to agent destruction, which
  // removes it from the instances.
  Instances copy = g_instances.Get();
  for (Instances::iterator it(copy.begin()); it != copy.end(); ++it) {
    DevToolsAgentHostImpl* agent_host = it->second;
    if (agent_host->client_) {
      scoped_refptr<DevToolsAgentHostImpl> protect(agent_host);
      // Clear |client_| before notifying it.
      DevToolsAgentHostClient* client = agent_host->client_;
      agent_host->client_ = NULL;
      DevToolsManager::GetInstance()->OnClientDetached();
      client->AgentHostClosed(agent_host, true);
      agent_host->Detach();
    }
  }
}

// static
void DevToolsAgentHost::AddAgentStateCallback(
    const AgentStateCallback& callback) {
  g_callbacks.Get().push_back(&callback);
}

// static
void DevToolsAgentHost::RemoveAgentStateCallback(
    const AgentStateCallback& callback) {
  if (g_callbacks == NULL)
    return;

  AgentStateCallbacks* callbacks_ = g_callbacks.Pointer();
  AgentStateCallbacks::iterator it =
      std::find(callbacks_->begin(), callbacks_->end(), &callback);
  DCHECK(it != callbacks_->end());
  callbacks_->erase(it);
}

// static
void DevToolsAgentHostImpl::NotifyCallbacks(
    DevToolsAgentHostImpl* agent_host, bool attached) {
  AgentStateCallbacks copy(g_callbacks.Get());
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->DevToolsAgentStateChanged(agent_host, attached);
  for (AgentStateCallbacks::iterator it = copy.begin(); it != copy.end(); ++it)
     (*it)->Run(agent_host, attached);
}

void DevToolsAgentHostImpl::Inspect(BrowserContext* browser_context) {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->Inspect(browser_context, this);
}

}  // namespace content
