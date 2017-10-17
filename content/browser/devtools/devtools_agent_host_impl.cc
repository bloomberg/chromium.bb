// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/forwarding_agent_host.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_agent_host.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> Instances;
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<DevToolsAgentHostObserver>>::Leaky
    g_observers = LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char DevToolsAgentHost::kTypePage[] = "page";
const char DevToolsAgentHost::kTypeFrame[] = "iframe";
const char DevToolsAgentHost::kTypeSharedWorker[] = "shared_worker";
const char DevToolsAgentHost::kTypeServiceWorker[] = "service_worker";
const char DevToolsAgentHost::kTypeBrowser[] = "browser";
const char DevToolsAgentHost::kTypeGuest[] = "webview";
const char DevToolsAgentHost::kTypeOther[] = "other";
int DevToolsAgentHostImpl::s_force_creation_count_ = 0;
int DevToolsAgentHostImpl::s_last_session_id_ = 0;

// static
std::string DevToolsAgentHost::GetProtocolVersion() {
  // TODO(dgozman): generate this.
  return "1.2";
}

// static
bool DevToolsAgentHost::IsSupportedProtocolVersion(const std::string& version) {
  // TODO(dgozman): generate this.
  return version == "1.0" || version == "1.1" || version == "1.2";
}

// static
DevToolsAgentHost::List DevToolsAgentHost::GetOrCreateAll() {
  List result;
  SharedWorkerDevToolsAgentHost::List shared_list;
  SharedWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&shared_list);
  for (const auto& host : shared_list)
    result.push_back(host);

  ServiceWorkerDevToolsAgentHost::List service_list;
  ServiceWorkerDevToolsManager::GetInstance()->AddAllAgentHosts(&service_list);
  for (const auto& host : service_list)
    result.push_back(host);

  RenderFrameDevToolsAgentHost::AddAllAgentHosts(&result);

#if DCHECK_IS_ON()
  for (auto it : result) {
    DevToolsAgentHostImpl* host = static_cast<DevToolsAgentHostImpl*>(it.get());
    DCHECK(g_instances.Get().find(host->id_) != g_instances.Get().end());
  }
#endif

  return result;
}

// Called on the UI thread.
// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForWorker(
    int worker_process_id,
    int worker_route_id) {
  if (scoped_refptr<DevToolsAgentHost> host =
      SharedWorkerDevToolsManager::GetInstance()
          ->GetDevToolsAgentHostForWorker(worker_process_id,
                                          worker_route_id)) {
    return host;
  }
  return ServiceWorkerDevToolsManager::GetInstance()
      ->GetDevToolsAgentHostForWorker(worker_process_id, worker_route_id);
}

DevToolsAgentHostImpl::DevToolsAgentHostImpl(const std::string& id) : id_(id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NotifyDestroyed();
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

// static
scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::Forward(
    const std::string& id,
    std::unique_ptr<DevToolsExternalAgentProxyDelegate> delegate) {
  scoped_refptr<DevToolsAgentHost> result = DevToolsAgentHost::GetForId(id);
  if (result)
    return result;
  return new ForwardingAgentHost(id, std::move(delegate));
}

DevToolsSession* DevToolsAgentHostImpl::SessionById(int session_id) {
  auto it = session_by_id_.find(session_id);
  return it == session_by_id_.end() ? nullptr : it->second;
}

DevToolsSession* DevToolsAgentHostImpl::SessionByClient(
    DevToolsAgentHostClient* client) {
  auto it = session_by_client_.find(client);
  return it == session_by_client_.end() ? nullptr : it->second.get();
}

void DevToolsAgentHostImpl::InnerAttachClient(DevToolsAgentHostClient* client) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  DevToolsSession* session =
      new DevToolsSession(this, client, ++s_last_session_id_);
  int session_id = session->session_id();
  sessions_.insert(session);
  session_by_id_[session_id] = session;
  session_by_client_[client].reset(session);
  AttachSession(session);
  if (sessions_.size() == 1)
    NotifyAttached();
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->SessionCreated(this, session_id);
}

void DevToolsAgentHostImpl::AttachClient(DevToolsAgentHostClient* client) {
  if (SessionByClient(client))
    return;
  InnerAttachClient(client);
}

void DevToolsAgentHostImpl::ForceAttachClient(DevToolsAgentHostClient* client) {
  if (SessionByClient(client))
    return;
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  if (!sessions_.empty())
    ForceDetachAllClients(true);
  DCHECK(sessions_.empty());
  InnerAttachClient(client);
}

bool DevToolsAgentHostImpl::DetachClient(DevToolsAgentHostClient* client) {
  if (!SessionByClient(client))
    return false;

  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  InnerDetachClient(client);
  return true;
}

bool DevToolsAgentHostImpl::DispatchProtocolMessage(
    DevToolsAgentHostClient* client,
    const std::string& message) {
  DevToolsSession* session = SessionByClient(client);
  if (!session)
    return false;
  return DispatchProtocolMessage(session, message);
}

bool DevToolsAgentHostImpl::SendProtocolMessageToClient(
    int session_id,
    const std::string& message) {
  DevToolsSession* session = SessionById(session_id);
  if (!session)
    return false;
  session->SendMessageToClient(message);
  return true;
}

void DevToolsAgentHostImpl::InnerDetachClient(DevToolsAgentHostClient* client) {
  DevToolsSession* session = SessionByClient(client);
  int session_id = session->session_id();
  sessions_.erase(session);
  session_by_id_.erase(session_id);
  session_by_client_.erase(client);
  DetachSession(session_id);
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate())
    manager->delegate()->SessionDestroyed(this, session_id);
  if (sessions_.empty()) {
    io_context_.DiscardAllStreams();
    NotifyDetached();
  }
}

bool DevToolsAgentHostImpl::IsAttached() {
  return !sessions_.empty();
}

void DevToolsAgentHostImpl::InspectElement(
    DevToolsAgentHostClient* client,
    int x,
    int y) {
  DevToolsSession* session = SessionByClient(client);
  if (session)
    InspectElement(session, x, y);
}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

std::string DevToolsAgentHostImpl::GetParentId() {
  return std::string();
}

std::string DevToolsAgentHostImpl::GetOpenerId() {
  return std::string();
}

std::string DevToolsAgentHostImpl::GetDescription() {
  return std::string();
}

GURL DevToolsAgentHostImpl::GetFaviconURL() {
  return GURL();
}

std::string DevToolsAgentHostImpl::GetFrontendURL() {
  return std::string();
}

base::TimeTicks DevToolsAgentHostImpl::GetLastActivityTime() {
  return base::TimeTicks();
}

BrowserContext* DevToolsAgentHostImpl::GetBrowserContext() {
  return nullptr;
}

WebContents* DevToolsAgentHostImpl::GetWebContents() {
  return NULL;
}

void DevToolsAgentHostImpl::DisconnectWebContents() {
}

void DevToolsAgentHostImpl::ConnectWebContents(WebContents* wc) {
}

bool DevToolsAgentHostImpl::Inspect() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->delegate()) {
    manager->delegate()->Inspect(this);
    return true;
  }
  return false;
}

void DevToolsAgentHostImpl::ForceDetachAllClients(bool replaced) {
  scoped_refptr<DevToolsAgentHostImpl> protect(this);
  while (!session_by_client_.empty()) {
    DevToolsAgentHostClient* client = session_by_client_.begin()->first;
    InnerDetachClient(client);
    client->AgentHostClosed(this, replaced);
  }
}

void DevToolsAgentHostImpl::ForceDetachSession(DevToolsSession* session) {
  DevToolsAgentHostClient* client = session->client();
  InnerDetachClient(client);
  client->AgentHostClosed(this, false);
}

void DevToolsAgentHostImpl::InspectElement(
    DevToolsSession* session,
    int x,
    int y) {
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
    agent_host->ForceDetachAllClients(true);
  }
}

// static
void DevToolsAgentHost::AddObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation()) {
    if (!DevToolsAgentHostImpl::s_force_creation_count_) {
      // Force all agent hosts when first observer is added.
      DevToolsAgentHost::GetOrCreateAll();
    }
    DevToolsAgentHostImpl::s_force_creation_count_++;
  }

  g_observers.Get().AddObserver(observer);
  for (const auto& id_host : g_instances.Get())
    observer->DevToolsAgentHostCreated(id_host.second);
}

// static
void DevToolsAgentHost::RemoveObserver(DevToolsAgentHostObserver* observer) {
  if (observer->ShouldForceDevToolsAgentHostCreation())
    DevToolsAgentHostImpl::s_force_creation_count_--;
  g_observers.Get().RemoveObserver(observer);
}

// static
bool DevToolsAgentHostImpl::ShouldForceCreation() {
  return !!s_force_creation_count_;
}

void DevToolsAgentHostImpl::NotifyCreated() {
  DCHECK(g_instances.Get().find(id_) == g_instances.Get().end());
  g_instances.Get()[id_] = this;
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostCreated(this);
}

void DevToolsAgentHostImpl::NotifyNavigated() {
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostNavigated(this);
}

void DevToolsAgentHostImpl::NotifyAttached() {
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostAttached(this);
}

void DevToolsAgentHostImpl::NotifyDetached() {
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostDetached(this);
}

void DevToolsAgentHostImpl::NotifyDestroyed() {
  DCHECK(g_instances.Get().find(id_) != g_instances.Get().end());
  for (auto& observer : g_observers.Get())
    observer.DevToolsAgentHostDestroyed(this);
  g_instances.Get().erase(id_);
}

// DevToolsMessageChunkProcessor -----------------------------------------------

DevToolsMessageChunkProcessor::DevToolsMessageChunkProcessor(
    const SendMessageCallback& callback)
    : callback_(callback),
      message_buffer_size_(0),
      last_call_id_(0) {
}

DevToolsMessageChunkProcessor::~DevToolsMessageChunkProcessor() {
}

bool DevToolsMessageChunkProcessor::ProcessChunkedMessageFromAgent(
    const DevToolsMessageChunk& chunk) {
  if (chunk.is_last && !chunk.post_state.empty())
    state_cookie_ = chunk.post_state;
  if (chunk.is_last)
    last_call_id_ = chunk.call_id;

  if (chunk.is_first && chunk.is_last) {
    if (message_buffer_size_ != 0)
      return false;
    callback_.Run(chunk.session_id, chunk.data);
    return true;
  }

  if (chunk.is_first) {
    message_buffer_ = std::string();
    message_buffer_.reserve(chunk.message_size);
    message_buffer_size_ = chunk.message_size;
  }

  if (message_buffer_.size() + chunk.data.size() > message_buffer_size_)
    return false;
  message_buffer_.append(chunk.data);

  if (chunk.is_last) {
    if (message_buffer_.size() != message_buffer_size_)
      return false;
    callback_.Run(chunk.session_id, message_buffer_);
    message_buffer_ = std::string();
    message_buffer_size_ = 0;
  }
  return true;
}

void DevToolsMessageChunkProcessor::Reset() {
  message_buffer_ = std::string();
  message_buffer_size_ = 0;
  state_cookie_ = std::string();
  last_call_id_ = 0;
}

}  // namespace content
