// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

namespace {
// Functor to sort by the .second element of a struct.
struct SecondGreater {
  template <typename Value>
  bool operator()(const Value& lhs, const Value& rhs) {
    return lhs.second > rhs.second;
  }
};
}  // namespace

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  if (status_ == STARTING || status_ == RUNNING)
    Stop();
  registry_->RemoveWorker(process_id_, embedded_worker_id_);
}

void EmbeddedWorkerInstance::Start(int64 service_worker_version_id,
                                   const GURL& scope,
                                   const GURL& script_url,
                                   const std::vector<int>& possible_process_ids,
                                   const StatusCallback& callback) {
  DCHECK(status_ == STOPPED);
  status_ = STARTING;
  std::vector<int> ordered_process_ids = SortProcesses(possible_process_ids);
  registry_->StartWorker(ordered_process_ids,
                         embedded_worker_id_,
                         service_worker_version_id,
                         scope,
                         script_url,
                         callback);
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == STARTING || status_ == RUNNING);
  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id_, embedded_worker_id_);
  if (status == SERVICE_WORKER_OK)
    status_ = STOPPING;
  return status;
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    const IPC::Message& message) {
  DCHECK(status_ == RUNNING);
  return registry_->Send(process_id_,
                         new EmbeddedWorkerContextMsg_MessageToWorker(
                             thread_id_, embedded_worker_id_, message));
}

void EmbeddedWorkerInstance::AddProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end())
    found = process_refs_.insert(std::make_pair(process_id, 0)).first;
  ++found->second;
}

void EmbeddedWorkerInstance::ReleaseProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end()) {
    NOTREACHED() << "Releasing unknown process ref " << process_id;
    return;
  }
  if (--found->second == 0)
    process_refs_.erase(found);
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(EmbeddedWorkerRegistry* registry,
                                               int embedded_worker_id)
    : registry_(registry),
      embedded_worker_id_(embedded_worker_id),
      status_(STOPPED),
      process_id_(-1),
      thread_id_(-1),
      worker_devtools_agent_route_id_(MSG_ROUTING_NONE) {
}

void EmbeddedWorkerInstance::RecordProcessId(
    int process_id,
    ServiceWorkerStatusCode status,
    int worker_devtools_agent_route_id) {
  DCHECK_EQ(process_id_, -1);
  DCHECK_EQ(worker_devtools_agent_route_id_, MSG_ROUTING_NONE);
  if (status == SERVICE_WORKER_OK) {
    process_id_ = process_id;
    worker_devtools_agent_route_id_ = worker_devtools_agent_route_id;
  } else {
    status_ = STOPPED;
  }
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  // TODO(horo): Implement this.
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
}

void EmbeddedWorkerInstance::OnStarted(int thread_id) {
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  status_ = RUNNING;
  thread_id_ = thread_id;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStarted());
}

void EmbeddedWorkerInstance::OnStopped() {
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  worker_devtools_agent_route_id_ = MSG_ROUTING_NONE;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopped());
}

bool EmbeddedWorkerInstance::OnMessageReceived(const IPC::Message& message) {
  ListenerList::Iterator it(listener_list_);
  while (Listener* listener = it.GetNext()) {
    if (listener->OnMessageReceived(message))
      return true;
  }
  return false;
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listener_list_,
      OnReportException(error_message, line_number, column_number, source_url));
}

void EmbeddedWorkerInstance::OnReportConsoleMessage(
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listener_list_,
      OnReportConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

void EmbeddedWorkerInstance::AddListener(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveListener(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

std::vector<int> EmbeddedWorkerInstance::SortProcesses(
    const std::vector<int>& possible_process_ids) const {
  // Add the |possible_process_ids| to the existing process_refs_ since each one
  // is likely to take a reference once the SW starts up.
  ProcessRefMap refs_with_new_ids = process_refs_;
  for (std::vector<int>::const_iterator it = possible_process_ids.begin();
       it != possible_process_ids.end();
       ++it) {
    refs_with_new_ids[*it]++;
  }

  std::vector<std::pair<int, int> > counted(refs_with_new_ids.begin(),
                                            refs_with_new_ids.end());
  // Sort descending by the reference count.
  std::sort(counted.begin(), counted.end(), SecondGreater());

  std::vector<int> result(counted.size());
  for (size_t i = 0; i < counted.size(); ++i)
    result[i] = counted[i].first;
  return result;
}

}  // namespace content
