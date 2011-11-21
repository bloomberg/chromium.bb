// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/worker_devtools_manager.h"

#include <list>
#include <map>

#include "base/bind.h"
#include "content/browser/debugger/devtools_agent_host.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/worker_devtools_message_filter.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using content::BrowserThread;

class WorkerDevToolsManager::AgentHosts
    : private content::NotificationObserver {
public:
  static void Add(WorkerId id, WorkerDevToolsAgentHost* host) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!instance_)
      instance_ = new AgentHosts();
    instance_->map_[id] = host;
  }
  static void Remove(WorkerId id) {
    DCHECK(instance_);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    Instances& map = instance_->map_;
    map.erase(id);
    if (map.empty()) {
      delete instance_;
      instance_ = NULL;
    }
  }

  static WorkerDevToolsAgentHost* GetAgentHost(WorkerId id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!instance_)
      return NULL;
    Instances& map = instance_->map_;
    Instances::iterator it = map.find(id);
    if (it == map.end())
      return NULL;
    return it->second;
  }

private:
  AgentHosts() {
    registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }
  ~AgentHosts() {}

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource&,
                       const content::NotificationDetails&) OVERRIDE;

  static AgentHosts* instance_;
  typedef std::map<WorkerId, WorkerDevToolsAgentHost*> Instances;
  Instances map_;
  content::NotificationRegistrar registrar_;
};

WorkerDevToolsManager::AgentHosts*
    WorkerDevToolsManager::AgentHosts::instance_ = NULL;


struct WorkerDevToolsManager::TerminatedInspectedWorker {
  TerminatedInspectedWorker(WorkerId id, const GURL& url, const string16& name)
      : old_worker_id(id),
        worker_url(url),
        worker_name(name) {}
  WorkerId old_worker_id;
  GURL worker_url;
  string16 worker_name;
};


class WorkerDevToolsManager::WorkerDevToolsAgentHost
    : public DevToolsAgentHost {
 public:
  explicit WorkerDevToolsAgentHost(WorkerId worker_id)
      : worker_id_(worker_id) {
    AgentHosts::Add(worker_id, this);
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            &RegisterAgent,
            worker_id.first,
            worker_id.second));
  }

  void WorkerDestroyed() {
    NotifyCloseListener();
    delete this;
  }

 private:
  virtual ~WorkerDevToolsAgentHost() {
    AgentHosts::Remove(worker_id_);
  }

  static void RegisterAgent(
      int worker_process_id,
      int worker_route_id) {
    WorkerDevToolsManager::GetInstance()->RegisterDevToolsAgentHostForWorker(
        worker_process_id, worker_route_id);
  }

  static void ForwardToWorkerDevToolsAgent(
      int worker_process_id,
      int worker_route_id,
      const IPC::Message& message) {
    WorkerDevToolsManager::GetInstance()->ForwardToWorkerDevToolsAgent(
        worker_process_id, worker_route_id, message);
  }

  // DevToolsAgentHost implementation.
  virtual void SendMessageToAgent(IPC::Message* message) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &WorkerDevToolsAgentHost::ForwardToWorkerDevToolsAgent,
            worker_id_.first,
            worker_id_.second,
            *message));
  }
  virtual void NotifyClientClosing() OVERRIDE {}
  virtual int GetRenderProcessId() OVERRIDE { return -1; }

  WorkerId worker_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentHost);
};


class WorkerDevToolsManager::DetachedClientHosts {
 public:
  static void WorkerReloaded(WorkerId old_id, WorkerId new_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (instance_ && instance_->ReattachClient(old_id, new_id))
      return;
    RemovePendingWorkerData(old_id);
  }

  static void WorkerDestroyed(WorkerId id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    WorkerDevToolsAgentHost* agent = AgentHosts::GetAgentHost(id);
    if (!agent) {
      RemovePendingWorkerData(id);
      return;
    }
    DevToolsManager::GetInstance()->ForwardToDevToolsClient(agent,
        DevToolsClientMsg_DispatchOnInspectorFrontend(MSG_ROUTING_NONE,
            WebKit::WebDevToolsAgent::disconnectEventAsText().utf8()));
    int cookie = DevToolsManager::GetInstance()->DetachClientHost(agent);
    if (cookie == -1) {
      RemovePendingWorkerData(id);
      return;
    }
    if (!instance_)
      new DetachedClientHosts();
    instance_->worker_id_to_cookie_[id] = cookie;
  }

 private:
  DetachedClientHosts() {
    instance_ = this;
  }
  ~DetachedClientHosts() {
    instance_ = NULL;
  }

  bool ReattachClient(WorkerId old_id, WorkerId new_id) {
    WorkerIdToCookieMap::iterator it = worker_id_to_cookie_.find(old_id);
    if (it == worker_id_to_cookie_.end())
      return false;
    DevToolsAgentHost* agent =
        WorkerDevToolsManager::GetDevToolsAgentHostForWorker(
            new_id.first,
            new_id.second);
    DevToolsManager::GetInstance()->AttachClientHost(
        it->second,
        agent);
    worker_id_to_cookie_.erase(it);
    if (worker_id_to_cookie_.empty())
      delete this;
    return true;
  }

  static void RemovePendingWorkerData(WorkerId id) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&RemoveInspectedWorkerDataOnIOThread, id));
  }

  static void RemoveInspectedWorkerDataOnIOThread(WorkerId id) {
    WorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(id);
  }

  static DetachedClientHosts* instance_;
  typedef std::map<WorkerId, int> WorkerIdToCookieMap;
  WorkerIdToCookieMap worker_id_to_cookie_;
};

WorkerDevToolsManager::DetachedClientHosts*
    WorkerDevToolsManager::DetachedClientHosts::instance_ = NULL;


void WorkerDevToolsManager::AgentHosts::Observe(
    int type,
    const content::NotificationSource&,
    const content::NotificationDetails&) {
  DCHECK(type == content::NOTIFICATION_APP_TERMINATING);
  Instances copy(map_);
  for (Instances::iterator it = copy.begin(); it != copy.end(); ++it)
    it->second->WorkerDestroyed();
  DCHECK(!instance_);
}

struct WorkerDevToolsManager::InspectedWorker {
  InspectedWorker(WorkerProcessHost* host, int route_id, const GURL& url,
                  const string16& name)
      : host(host),
        route_id(route_id),
        worker_url(url),
        worker_name(name) {}
  WorkerProcessHost* const host;
  int const route_id;
  GURL worker_url;
  string16 worker_name;
};

// static
WorkerDevToolsManager* WorkerDevToolsManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<WorkerDevToolsManager>::get();
}

// static
DevToolsAgentHost* WorkerDevToolsManager::GetDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  WorkerId id(worker_process_id, worker_route_id);
  WorkerDevToolsAgentHost* result = AgentHosts::GetAgentHost(id);
  if (!result)
    result = new WorkerDevToolsAgentHost(id);
  return result;
}

WorkerDevToolsManager::WorkerDevToolsManager() {
  WorkerService::GetInstance()->AddObserver(this);
}

WorkerDevToolsManager::~WorkerDevToolsManager() {
}

void WorkerDevToolsManager::WorkerCreated(
    WorkerProcessHost* worker,
    const WorkerProcessHost::WorkerInstance& instance) {
  for (TerminatedInspectedWorkers::iterator it = terminated_workers_.begin();
       it != terminated_workers_.end(); ++it) {
    if (instance.Matches(it->worker_url, it->worker_name,
                         instance.resource_context())) {
      worker->Send(new DevToolsAgentMsg_PauseWorkerContextOnStart(
          instance.worker_route_id()));
      WorkerId new_worker_id(worker->id(), instance.worker_route_id());
      paused_workers_[new_worker_id] = it->old_worker_id;
      terminated_workers_.erase(it);
      return;
    }
  }
}

void WorkerDevToolsManager::WorkerDestroyed(
    WorkerProcessHost* worker,
    int worker_route_id) {
  InspectedWorkersList::iterator it = FindInspectedWorker(
      worker->id(),
      worker_route_id);
  if (it == inspected_workers_.end())
    return;

  WorkerId worker_id(worker->id(), worker_route_id);
  terminated_workers_.push_back(TerminatedInspectedWorker(
      worker_id,
      it->worker_url,
      it->worker_name));
  inspected_workers_.erase(it);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DetachedClientHosts::WorkerDestroyed, worker_id));
}

void WorkerDevToolsManager::WorkerContextStarted(WorkerProcessHost* process,
                                                 int worker_route_id) {
  WorkerId new_worker_id(process->id(), worker_route_id);
  PausedWorkers::iterator it = paused_workers_.find(new_worker_id);
  if (it == paused_workers_.end())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DetachedClientHosts::WorkerReloaded,
          it->second,
          new_worker_id));
  paused_workers_.erase(it);
}

void WorkerDevToolsManager::RemoveInspectedWorkerData(
    const WorkerId& id) {
  for (TerminatedInspectedWorkers::iterator it = terminated_workers_.begin();
       it != terminated_workers_.end(); ++it) {
    if (it->old_worker_id == id) {
      terminated_workers_.erase(it);
      return;
    }
  }

  for (PausedWorkers::iterator it = paused_workers_.begin();
       it != paused_workers_.end(); ++it) {
    if (it->second == id) {
      SendResumeToWorker(it->first);
      paused_workers_.erase(it);
      return;
    }
  }
}

WorkerDevToolsManager::InspectedWorkersList::iterator
WorkerDevToolsManager::FindInspectedWorker(
    int host_id, int route_id) {
  InspectedWorkersList::iterator it = inspected_workers_.begin();
  while (it != inspected_workers_.end()) {
    if (it->host->id() == host_id && it->route_id == route_id)
      break;
    ++it;
  }
  return it;
}

static WorkerProcessHost* FindWorkerProcess(int worker_process_id) {
  BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
  for (; !iter.Done(); ++iter) {
    if (iter->id() == worker_process_id)
      return static_cast<WorkerProcessHost*>(*iter);
  }
  return NULL;
}

void WorkerDevToolsManager::RegisterDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  if (WorkerProcessHost* process = FindWorkerProcess(worker_process_id)) {
    const WorkerProcessHost::Instances& instances = process->instances();
    for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
         i != instances.end(); ++i) {
      if (i->worker_route_id() == worker_route_id) {
        DCHECK(FindInspectedWorker(worker_process_id, worker_route_id) ==
               inspected_workers_.end());
        inspected_workers_.push_back(
            InspectedWorker(process, worker_route_id, i->url(), i->name()));
        return;
      }
    }
  }
  NotifyWorkerDestroyedOnIOThread(worker_process_id, worker_route_id);
}

void WorkerDevToolsManager::ForwardToDevToolsClient(
    int worker_process_id,
    int worker_route_id,
    const IPC::Message& message) {
  if (FindInspectedWorker(worker_process_id, worker_route_id) ==
          inspected_workers_.end()) {
      NOTREACHED();
      return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &ForwardToDevToolsClientOnUIThread,
          worker_process_id,
          worker_route_id,
          message));
}

void WorkerDevToolsManager::SaveAgentRuntimeState(int worker_process_id,
                                                  int worker_route_id,
                                                  const std::string& state) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &SaveAgentRuntimeStateOnUIThread,
          worker_process_id,
          worker_route_id,
          state));
}

void WorkerDevToolsManager::ForwardToWorkerDevToolsAgent(
    int worker_process_id,
    int worker_route_id,
    const IPC::Message& message) {
  InspectedWorkersList::iterator it = FindInspectedWorker(
      worker_process_id,
      worker_route_id);
  if (it == inspected_workers_.end())
    return;
  IPC::Message* msg = new IPC::Message(message);
  msg->set_routing_id(worker_route_id);
  it->host->Send(msg);
}

// static
void WorkerDevToolsManager::ForwardToDevToolsClientOnUIThread(
    int worker_process_id,
    int worker_route_id,
    const IPC::Message& message) {
  WorkerDevToolsAgentHost* agent_host = AgentHosts::GetAgentHost(WorkerId(
      worker_process_id,
      worker_route_id));
  if (!agent_host)
    return;
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(agent_host, message);
}

// static
void WorkerDevToolsManager::SaveAgentRuntimeStateOnUIThread(
    int worker_process_id,
    int worker_route_id,
    const std::string& state) {
  WorkerDevToolsAgentHost* agent_host = AgentHosts::GetAgentHost(WorkerId(
      worker_process_id,
      worker_route_id));
  if (!agent_host)
    return;
  DevToolsManager::GetInstance()->SaveAgentRuntimeState(agent_host, state);
}

// static
void WorkerDevToolsManager::NotifyWorkerDestroyedOnIOThread(
    int worker_process_id,
    int worker_route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WorkerDevToolsManager::NotifyWorkerDestroyedOnUIThread,
          worker_process_id,
          worker_route_id));
}

// static
void WorkerDevToolsManager::NotifyWorkerDestroyedOnUIThread(
    int worker_process_id,
    int worker_route_id) {
  WorkerDevToolsAgentHost* host =
      AgentHosts::GetAgentHost(WorkerId(worker_process_id, worker_route_id));
  if (host)
    host->WorkerDestroyed();
}

// static
void WorkerDevToolsManager::SendResumeToWorker(const WorkerId& id) {
  if (WorkerProcessHost* process = FindWorkerProcess(id.first))
    process->Send(new DevToolsAgentMsg_ResumeWorkerContext(id.second));
}
