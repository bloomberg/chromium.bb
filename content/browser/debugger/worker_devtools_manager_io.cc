// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/worker_devtools_manager_io.h"

#include <list>

#include "base/tuple.h"
#include "content/browser/browser_thread.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/worker_devtools_message_filter.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/devtools_messages.h"

namespace {

void NotifyWorkerDevToolsClientClosingOnIOThread(int worker_process_host_id,
                                                 int worker_route_id) {
  WorkerDevToolsManagerIO::GetInstance()->WorkerDevToolsClientClosing(
      worker_process_host_id,
      worker_route_id);
}

class ClientsUI : public DevToolsClientHost::CloseListener {
 public:
  static ClientsUI* GetInstance() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    static ClientsUI* instance = new ClientsUI();
    return instance;
  }

  DevToolsClientHost* FindDevToolsClient(int worker_process_host_id,
                                         int worker_route_id) {
    WorkerIdToClientHostMap::iterator it =
        worker_id_to_client_host_.find(WorkerId(worker_process_host_id,
                                                worker_route_id));
    if (it == worker_id_to_client_host_.end())
      return NULL;
    return it->second;
  }

  bool FindWorkerInfo(DevToolsClientHost* client_host,
                      int* host_id,
                      int* route_id) {
    for (WorkerIdToClientHostMap::const_iterator it =
             worker_id_to_client_host_.begin();
         it != worker_id_to_client_host_.end(); ++it) {
      if (it->second == client_host) {
        WorkerId id = it->first;
        *host_id = id.first;
        *route_id = id.second;
        return true;
      }
    }
    return false;
  }

  void RegisterDevToolsClientForWorker(int worker_process_host_id,
                                       int worker_route_id,
                                       DevToolsClientHost* client) {
    client->set_close_listener(this);
    WorkerId worker_id(worker_process_host_id, worker_route_id);
    worker_id_to_client_host_[worker_id] = client;
  }

 private:
  ClientsUI() {}
  virtual ~ClientsUI() {}

  virtual void ClientHostClosing(DevToolsClientHost* host) {
    WorkerIdToClientHostMap::iterator it = worker_id_to_client_host_.begin();
    for (; it != worker_id_to_client_host_.end(); ++it) {
      if (it->second == host) {
        WorkerId id = it->first;
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            NewRunnableFunction(NotifyWorkerDevToolsClientClosingOnIOThread,
                                id.first, id.second));
        worker_id_to_client_host_.erase(it);
        return;
      }
    }
  }

  typedef std::pair<int, int> WorkerId;
  typedef std::map<WorkerId, DevToolsClientHost*> WorkerIdToClientHostMap;
  WorkerIdToClientHostMap worker_id_to_client_host_;

  DISALLOW_COPY_AND_ASSIGN(ClientsUI);
};

}  // namespace

class WorkerDevToolsManagerIO::InspectedWorkersList {
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

  void WorkerDevToolsMessageFilterClosing(int worker_process_host_id) {
    Entries::iterator it = map_.begin();
    while (it != map_.end()) {
      if (it->host->id() == worker_process_host_id)
        it = map_.erase(it);
      else
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
WorkerDevToolsManagerIO* WorkerDevToolsManagerIO::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<WorkerDevToolsManagerIO>::get();
}

WorkerDevToolsManagerIO::WorkerDevToolsManagerIO()
    : inspected_workers_(new InspectedWorkersList()) {
}

WorkerDevToolsManagerIO::~WorkerDevToolsManagerIO() {
}

bool WorkerDevToolsManagerIO::ForwardToWorkerDevToolsAgentOnUIThread(
    DevToolsClientHost* from,
    const IPC::Message& message) {
  int worker_process_host_id;
  int worker_route_id;
  if (!ClientsUI::GetInstance()->FindWorkerInfo(
          from, &worker_process_host_id, &worker_route_id))
    return false;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          ForwardToWorkerDevToolsAgentOnIOThread,
          worker_process_host_id,
          worker_route_id,
          IPC::Message(message)));
  return true;
}

static WorkerProcessHost* FindWorkerProcessHostForWorker(
    int worker_process_host_id,
    int worker_route_id) {
  BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
  for (; !iter.Done(); ++iter) {
    if (iter->id() != worker_process_host_id)
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

// static
bool WorkerDevToolsManagerIO::HasDevToolsClient(
    int worker_process_id,
    int worker_route_id) {
  return NULL != ClientsUI::GetInstance()->
      FindDevToolsClient(worker_process_id, worker_route_id);
}

// static
void WorkerDevToolsManagerIO::RegisterDevToolsClientForWorkerOnUIThread(
      DevToolsClientHost* client,
      int worker_process_id,
      int worker_route_id) {
  DCHECK(!HasDevToolsClient(worker_process_id, worker_route_id));
  ClientsUI::GetInstance()->RegisterDevToolsClientForWorker(
      worker_process_id, worker_route_id, client);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(
          WorkerDevToolsManagerIO::RegisterDevToolsClientForWorker,
          worker_process_id,
          worker_route_id));
}

// static
void WorkerDevToolsManagerIO::RegisterDevToolsClientForWorker(
    int worker_process_id,
    int worker_route_id) {
  WorkerDevToolsManagerIO::GetInstance()->AddInspectedInstance(
      worker_process_id,
      worker_route_id);
}

void WorkerDevToolsManagerIO::AddInspectedInstance(
    int worker_process_host_id,
    int worker_route_id) {
  WorkerProcessHost* host = FindWorkerProcessHostForWorker(
      worker_process_host_id,
      worker_route_id);
  if (!host)
    return;

  // DevTools client is already attached.
  if (inspected_workers_->Contains(worker_process_host_id, worker_route_id))
    return;

  host->Send(new WorkerDevToolsAgentMsg_Attach(worker_route_id));

  inspected_workers_->AddInstance(host, worker_route_id);
}

void WorkerDevToolsManagerIO::WorkerDevToolsClientClosing(
    int worker_process_host_id,
    int worker_route_id) {
  WorkerProcessHost* host = inspected_workers_->RemoveInstance(
      worker_process_host_id, worker_route_id);
  if (host)
    host->Send(new WorkerDevToolsAgentMsg_Detach(worker_route_id));
}

static void ForwardToDevToolsClientOnUIThread(
    int worker_process_host_id,
    int worker_route_id,
    const IPC::Message& message) {
  DevToolsClientHost* client = ClientsUI::GetInstance()->
      FindDevToolsClient(worker_process_host_id, worker_route_id);
  if (client)
    client->SendMessageToClient(message);
}

void WorkerDevToolsManagerIO::ForwardToDevToolsClient(
    int worker_process_host_id,
    int worker_route_id,
    const IPC::Message& message) {
  if (!inspected_workers_->Contains(worker_process_host_id, worker_route_id)) {
      NOTREACHED();
      return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          ForwardToDevToolsClientOnUIThread,
          worker_process_host_id,
          worker_route_id,
          IPC::Message(message)));
}

void WorkerDevToolsManagerIO::WorkerProcessDestroying(
    int worker_process_host_id) {
  inspected_workers_->WorkerDevToolsMessageFilterClosing(
      worker_process_host_id);
}

void WorkerDevToolsManagerIO::ForwardToWorkerDevToolsAgentOnIOThread(
    int worker_process_host_id,
    int worker_route_id,
    const IPC::Message& message) {
  WorkerDevToolsManagerIO::GetInstance()->ForwardToWorkerDevToolsAgent(
      worker_process_host_id, worker_route_id, message);
}

void WorkerDevToolsManagerIO::ForwardToWorkerDevToolsAgent(
    int worker_process_host_id,
    int worker_route_id,
    const IPC::Message& message) {
  WorkerProcessHost* host = inspected_workers_->FindHost(
      worker_process_host_id,
      worker_route_id);
  if (!host)
    return;
  IPC::Message* msg = new IPC::Message(message);
  msg->set_routing_id(worker_route_id);
  host->Send(msg);
}
