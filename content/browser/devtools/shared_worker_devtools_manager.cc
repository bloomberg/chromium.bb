// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_manager.h"

#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/ipc_devtools_agent_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/worker_service.h"
#include "ipc/ipc_listener.h"

namespace content {

namespace {

bool SendMessageToWorker(const SharedWorkerDevToolsManager::WorkerId& worker_id,
                         IPC::Message* message) {
  RenderProcessHost* host = RenderProcessHost::FromID(worker_id.first);
  if (!host) {
    delete message;
    return false;
  }
  message->set_routing_id(worker_id.second);
  host->Send(message);
  return true;
}

}  // namespace

class SharedWorkerDevToolsManager::SharedWorkerDevToolsAgentHost
    : public IPCDevToolsAgentHost,
      public IPC::Listener {
 public:
  explicit SharedWorkerDevToolsAgentHost(WorkerId worker_id)
      : worker_id_(worker_id), worker_attached_(true) {
    AddRef();
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
      host->AddRoute(worker_id_.second, this);
  }

  // IPCDevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* message) OVERRIDE {
    if (worker_attached_)
      SendMessageToWorker(worker_id_, message);
  }
  virtual void OnClientAttached() OVERRIDE {}
  virtual void OnClientDetached() OVERRIDE {}

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SharedWorkerDevToolsAgentHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                        OnSaveAgentRuntimeState)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void ReattachToWorker(WorkerId worker_id) {
    CHECK(!worker_attached_);
    worker_attached_ = true;
    worker_id_ = worker_id;
    AddRef();
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
      host->AddRoute(worker_id_.second, this);
    Reattach(state_);
  }

  void DetachFromWorker() {
    CHECK(worker_attached_);
    worker_attached_ = false;
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
      host->RemoveRoute(worker_id_.second);
    Release();
  }

  WorkerId worker_id() const { return worker_id_; }

 private:
  virtual ~SharedWorkerDevToolsAgentHost() {
    CHECK(!worker_attached_);
    SharedWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(this);
  }

  void OnDispatchOnInspectorFrontend(const std::string& message) {
    DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(this,
                                                                    message);
  }

  void OnSaveAgentRuntimeState(const std::string& state) { state_ = state; }

  WorkerId worker_id_;
  bool worker_attached_;
  std::string state_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsAgentHost);
};

// static
SharedWorkerDevToolsManager* SharedWorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<SharedWorkerDevToolsManager>::get();
}

DevToolsAgentHost* SharedWorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  WorkerId id(worker_process_id, worker_route_id);

  WorkerInfoMap::iterator it = workers_.find(id);
  if (it == workers_.end())
    return NULL;

  WorkerInfo* info = it->second;
  if (info->state() != WORKER_UNINSPECTED)
    return info->agent_host();

  SharedWorkerDevToolsAgentHost* agent_host =
      new SharedWorkerDevToolsAgentHost(id);
  info->set_agent_host(agent_host);
  info->set_state(WORKER_INSPECTED);
  return agent_host;
}

SharedWorkerDevToolsManager::SharedWorkerDevToolsManager() {}

SharedWorkerDevToolsManager::~SharedWorkerDevToolsManager() {}

bool SharedWorkerDevToolsManager::WorkerCreated(
    int worker_process_id,
    int worker_route_id,
    const SharedWorkerInstance& instance) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = FindExistingWorkerInfo(instance);
  if (it == workers_.end()) {
    scoped_ptr<WorkerInfo> info(new WorkerInfo(instance));
    workers_.set(id, info.Pass());
    return false;
  }
  DCHECK_EQ(WORKER_TERMINATED, it->second->state());
  scoped_ptr<WorkerInfo> info = workers_.take_and_erase(it);
  info->set_state(WORKER_PAUSED);
  workers_.set(id, info.Pass());
  return true;
}

void SharedWorkerDevToolsManager::WorkerDestroyed(int worker_process_id,
                                                  int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  WorkerInfo* info = it->second;
  switch (info->state()) {
    case WORKER_UNINSPECTED:
      workers_.erase(it);
      break;
    case WORKER_INSPECTED: {
      SharedWorkerDevToolsAgentHost* agent_host = info->agent_host();
      if (!agent_host->IsAttached()) {
        scoped_ptr<WorkerInfo> worker_info = workers_.take_and_erase(it);
        agent_host->DetachFromWorker();
        return;
      }
      info->set_state(WORKER_TERMINATED);
      // Client host is debugging this worker agent host.
      std::string notification =
          DevToolsProtocol::CreateNotification(
              devtools::Worker::disconnectedFromWorker::kName, NULL)
              ->Serialize();
      DevToolsManagerImpl::GetInstance()->DispatchOnInspectorFrontend(
          agent_host, notification);
      agent_host->DetachFromWorker();
      break;
    }
    case WORKER_TERMINATED:
      NOTREACHED();
      break;
    case WORKER_PAUSED: {
      scoped_ptr<WorkerInfo> worker_info = workers_.take_and_erase(it);
      worker_info->set_state(WORKER_TERMINATED);
      const WorkerId old_id = worker_info->agent_host()->worker_id();
      workers_.set(old_id, worker_info.Pass());
      break;
    }
  }
}

void SharedWorkerDevToolsManager::WorkerContextStarted(int worker_process_id,
                                                       int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(worker_process_id, worker_route_id);
  WorkerInfoMap::iterator it = workers_.find(id);
  DCHECK(it != workers_.end());
  WorkerInfo* info = it->second;
  if (info->state() != WORKER_PAUSED)
    return;
  info->agent_host()->ReattachToWorker(id);
  info->set_state(WORKER_INSPECTED);
}

void SharedWorkerDevToolsManager::RemoveInspectedWorkerData(
    SharedWorkerDevToolsAgentHost* agent_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const WorkerId id(agent_host->worker_id());
  scoped_ptr<WorkerInfo> worker_info = workers_.take_and_erase(id);
  if (worker_info) {
    DCHECK_EQ(WORKER_TERMINATED, worker_info->state());
    return;
  }
  for (WorkerInfoMap::iterator it = workers_.begin(); it != workers_.end();
       ++it) {
    if (it->second->agent_host() == agent_host) {
      DCHECK_EQ(WORKER_PAUSED, it->second->state());
      SendMessageToWorker(
          it->first,
          new DevToolsAgentMsg_ResumeWorkerContext(it->first.second));
      it->second->set_agent_host(NULL);
      it->second->set_state(WORKER_UNINSPECTED);
      return;
    }
  }
}

SharedWorkerDevToolsManager::WorkerInfoMap::iterator
SharedWorkerDevToolsManager::FindExistingWorkerInfo(
    const SharedWorkerInstance& instance) {
  WorkerInfoMap::iterator it = workers_.begin();
  for (; it != workers_.end(); ++it) {
    if (it->second->instance().Matches(instance))
      break;
  }
  return it;
}

void SharedWorkerDevToolsManager::ResetForTesting() { workers_.clear(); }

}  // namespace content
