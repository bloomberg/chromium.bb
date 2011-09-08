// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/worker_devtools_manager.h"

#include <list>
#include <map>

#include "base/tuple.h"
#include "content/browser/browser_thread.h"
#include "content/browser/debugger/devtools_agent_host.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/worker_devtools_message_filter.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/content_notification_types.h"
#include "content/common/devtools_messages.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

namespace {
typedef std::pair<int, int> WorkerId;
}

class WorkerDevToolsManager::AgentHosts : private NotificationObserver {
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
                   NotificationService::AllSources());
  }
  ~AgentHosts() {}

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource&,
                       const NotificationDetails&) OVERRIDE;

  static AgentHosts* instance_;
  typedef std::map<WorkerId, WorkerDevToolsAgentHost*> Instances;
  Instances map_;
  NotificationRegistrar registrar_;
};

WorkerDevToolsManager::AgentHosts*
    WorkerDevToolsManager::AgentHosts::instance_ = NULL;


class WorkerDevToolsManager::WorkerDevToolsAgentHost
    : public DevToolsAgentHost {
 public:
  explicit WorkerDevToolsAgentHost(WorkerId worker_id)
      : worker_id_(worker_id) {
    AgentHosts::Add(worker_id, this);
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableFunction(
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
        NewRunnableFunction(
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

void WorkerDevToolsManager::AgentHosts::Observe(int type,
                                                const NotificationSource&,
                                                const NotificationDetails&) {
  DCHECK(type == content::NOTIFICATION_APP_TERMINATING);
  Instances copy(map_);
  for (Instances::iterator it = copy.begin(); it != copy.end(); ++it)
    it->second->WorkerDestroyed();
  DCHECK(!instance_);
}

class WorkerDevToolsManager::InspectedWorkersList {
 public:
  InspectedWorkersList() {}

  void AddInstance(WorkerProcessHost* host, int route_id) {
    DCHECK(!Contains(host->id(), route_id));
    map_.push_back(Entry(host, route_id));
  }

  bool Contains(int host_id, int route_id) {
    return FindHost(host_id, route_id) != NULL;
  }

  WorkerProcessHost* FindHost(int host_id, int route_id) {
    Entries::iterator it = FindEntry(host_id, route_id);
    if (it == map_.end())
      return NULL;
    return it->host;
  }

  WorkerProcessHost* RemoveInstance(int host_id, int route_id) {
    Entries::iterator it = FindEntry(host_id, route_id);
    if (it == map_.end())
      return NULL;
    WorkerProcessHost* host = it->host;
    map_.erase(it);
    return host;
  }

  void WorkerDevToolsMessageFilterClosing(int worker_process_id) {
    Entries::iterator it = map_.begin();
    while (it != map_.end()) {
      if (it->host->id() == worker_process_id) {
        NotifyWorkerDestroyedOnIOThread(
                it->host->id(),
                it->route_id);
        it = map_.erase(it);
      } else
        ++it;
    }
  }

 private:
  struct Entry {
    Entry(WorkerProcessHost* host, int route_id)
        : host(host),
          route_id(route_id) {}
    WorkerProcessHost* const host;
    int const route_id;
  };
  typedef std::list<Entry> Entries;

  Entries::iterator FindEntry(int host_id, int route_id) {
    Entries::iterator it = map_.begin();
    while (it != map_.end()) {
      if (it->host->id() == host_id && it->route_id == route_id)
        break;
      ++it;
    }
    return it;
  }

  Entries map_;
  DISALLOW_COPY_AND_ASSIGN(InspectedWorkersList);
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

WorkerDevToolsManager::WorkerDevToolsManager()
    : inspected_workers_(new InspectedWorkersList()) {
}

WorkerDevToolsManager::~WorkerDevToolsManager() {
}

static WorkerProcessHost* FindWorkerProcessHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
  for (; !iter.Done(); ++iter) {
    if (iter->id() != worker_process_id)
      continue;
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    const WorkerProcessHost::Instances& instances = worker->instances();
    for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
         i != instances.end(); ++i) {
      if (i->shared() && i->worker_route_id() == worker_route_id)
        return worker;
    }
    return NULL;
  }
  return NULL;
}

void WorkerDevToolsManager::RegisterDevToolsAgentHostForWorker(
    int worker_process_id,
    int worker_route_id) {
  WorkerProcessHost* host = FindWorkerProcessHostForWorker(
      worker_process_id,
      worker_route_id);
  if (host)
    inspected_workers_->AddInstance(host, worker_route_id);
  else
    NotifyWorkerDestroyedOnIOThread(worker_process_id, worker_route_id);
}

void WorkerDevToolsManager::ForwardToDevToolsClient(
    int worker_process_id,
    int worker_route_id,
    const IPC::Message& message) {
  if (!inspected_workers_->Contains(worker_process_id, worker_route_id)) {
      NOTREACHED();
      return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          ForwardToDevToolsClientOnUIThread,
          worker_process_id,
          worker_route_id,
          message));
}

void WorkerDevToolsManager::WorkerProcessDestroying(
    int worker_process_id) {
  inspected_workers_->WorkerDevToolsMessageFilterClosing(
      worker_process_id);
}

void WorkerDevToolsManager::ForwardToWorkerDevToolsAgent(
    int worker_process_id,
    int worker_route_id,
    const IPC::Message& message) {
  WorkerProcessHost* host = inspected_workers_->FindHost(
      worker_process_id,
      worker_route_id);
  if (!host)
    return;
  IPC::Message* msg = new IPC::Message(message);
  msg->set_routing_id(worker_route_id);
  host->Send(msg);
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
void WorkerDevToolsManager::NotifyWorkerDestroyedOnIOThread(
    int worker_process_id,
    int worker_route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
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
