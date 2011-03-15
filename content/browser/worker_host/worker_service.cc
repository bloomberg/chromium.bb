// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_service.h"

#include <string>

#include "base/command_line.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/worker_messages.h"
#include "net/base/registry_controlled_domain.h"

const int WorkerService::kMaxWorkerProcessesWhenSharing = 10;
const int WorkerService::kMaxWorkersWhenSeparate = 64;
const int WorkerService::kMaxWorkersPerTabWhenSeparate = 16;

WorkerService* WorkerService::GetInstance() {
  return Singleton<WorkerService>::get();
}

WorkerService::WorkerService() : next_worker_route_id_(0) {
}

WorkerService::~WorkerService() {
}

void WorkerService::OnWorkerMessageFilterClosing(WorkerMessageFilter* filter) {
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
     !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    worker->FilterShutdown(filter);
  }

  // See if that process had any queued workers.
  for (WorkerProcessHost::Instances::iterator i = queued_workers_.begin();
       i != queued_workers_.end();) {
    i->RemoveFilters(filter);
    if (i->NumFilters() == 0) {
      i = queued_workers_.erase(i);
    } else {
      ++i;
    }
  }

  // Also, see if that process had any pending shared workers.
  for (WorkerProcessHost::Instances::iterator iter =
           pending_shared_workers_.begin();
       iter != pending_shared_workers_.end(); ) {
    iter->worker_document_set()->RemoveAll(filter);
    if (iter->worker_document_set()->IsEmpty()) {
      iter = pending_shared_workers_.erase(iter);
    } else {
      ++iter;
    }
  }

  // Either a worker proceess has shut down, in which case we can start one of
  // the queued workers, or a renderer has shut down, in which case it doesn't
  // affect anything.  We call this function in both scenarios because then we
  // don't have to keep track which filters are from worker processes.
  TryStartingQueuedWorker();
}

void WorkerService::CreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                                 int route_id,
                                 WorkerMessageFilter* filter,
                                 URLRequestContextGetter* request_context) {

  ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
      request_context->GetURLRequestContext());

  // Generate a unique route id for the browser-worker communication that's
  // unique among all worker processes.  That way when the worker process sends
  // a wrapped IPC message through us, we know which WorkerProcessHost to give
  // it to.
  WorkerProcessHost::WorkerInstance instance(
      params.url,
      params.is_shared,
      context->is_incognito(),
      params.name,
      next_worker_route_id(),
      params.is_shared ? 0 : filter->render_process_id(),
      params.is_shared ? 0 : params.parent_appcache_host_id,
      params.is_shared ? params.script_resource_appcache_id : 0,
      request_context);
  instance.AddFilter(filter, route_id);
  instance.worker_document_set()->Add(
      filter, params.document_id, filter->render_process_id(),
      params.render_view_route_id);

  CreateWorkerFromInstance(instance);
}

void WorkerService::LookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int route_id,
    WorkerMessageFilter* filter,
    bool incognito,
    bool* exists,
    bool* url_mismatch) {

  *exists = true;
  WorkerProcessHost::WorkerInstance* instance = FindSharedWorkerInstance(
      params.url, params.name, incognito);

  if (!instance) {
    // If no worker instance currently exists, we need to create a pending
    // instance - this is to make sure that any subsequent lookups passing a
    // mismatched URL get the appropriate url_mismatch error at lookup time.
    // Having named shared workers was a Really Bad Idea due to details like
    // this.
    instance = CreatePendingInstance(params.url, params.name, incognito);
    *exists = false;
  }

  // Make sure the passed-in instance matches the URL - if not, return an
  // error.
  if (params.url != instance->url()) {
    *url_mismatch = true;
    *exists = false;
  } else {
    *url_mismatch = false;
    // Add our route ID to the existing instance so we can send messages to it.
    instance->AddFilter(filter, route_id);

    // Add the passed filter/document_id to the worker instance.
    // TODO(atwilson): This won't work if the message is from a worker process.
    // We don't support that yet though (this message is only sent from
    // renderers) but when we do, we'll need to add code to pass in the current
    // worker's document set for nested workers.
    instance->worker_document_set()->Add(
        filter, params.document_id, filter->render_process_id(),
        params.render_view_route_id);
  }
}

void WorkerService::CancelCreateDedicatedWorker(
    int route_id,
    WorkerMessageFilter* filter) {
  for (WorkerProcessHost::Instances::iterator i = queued_workers_.begin();
       i != queued_workers_.end(); ++i) {
    if (i->HasFilter(filter, route_id)) {
      DCHECK(!i->shared());
      queued_workers_.erase(i);
      return;
    }
  }

  // There could be a race condition where the WebWorkerProxy told us to cancel
  // the worker right as we sent it a message say it's been created.  Look at
  // the running workers.
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    for (WorkerProcessHost::Instances::const_iterator instance =
             worker->instances().begin();
         instance != worker->instances().end(); ++instance) {
      if (instance->HasFilter(filter, route_id)) {
        // Fake a worker destroyed message so that WorkerProcessHost cleans up
        // properly.
        WorkerMsg_TerminateWorkerContext message(route_id);
        ForwardToWorker(message, filter);
        return;
      }
    }
  }

  DCHECK(false) << "Couldn't find worker to cancel";
}

void WorkerService::ForwardToWorker(const IPC::Message& message,
                                    WorkerMessageFilter* filter) {
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    if (worker->FilterMessage(message, filter))
      return;
  }

  // TODO(jabdelmalek): tell filter that callee is gone
}

void WorkerService::DocumentDetached(unsigned long long document_id,
                                     WorkerMessageFilter* filter) {
  // Any associated shared workers can be shut down.
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    worker->DocumentDetached(filter, document_id);
  }

  // Remove any queued shared workers for this document.
  for (WorkerProcessHost::Instances::iterator iter = queued_workers_.begin();
       iter != queued_workers_.end();) {
    if (iter->shared()) {
      iter->worker_document_set()->Remove(filter, document_id);
      if (iter->worker_document_set()->IsEmpty()) {
        iter = queued_workers_.erase(iter);
        continue;
      }
    }
    ++iter;
  }

  // Remove the document from any pending shared workers.
  for (WorkerProcessHost::Instances::iterator iter =
           pending_shared_workers_.begin();
       iter != pending_shared_workers_.end(); ) {
    iter->worker_document_set()->Remove(filter, document_id);
    if (iter->worker_document_set()->IsEmpty()) {
      iter = pending_shared_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

bool WorkerService::CreateWorkerFromInstance(
    WorkerProcessHost::WorkerInstance instance) {
  // TODO(michaeln): We need to ensure that a process is working
  // on behalf of a single profile. The process sharing logic below
  // does not ensure that. Consider making WorkerService a per profile
  // object to help with this.
  WorkerProcessHost* worker = NULL;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebWorkerProcessPerCore)) {
    worker = GetProcessToFillUpCores();
  } else if (CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kWebWorkerShareProcesses)) {
    worker = GetProcessForDomain(instance.url());
  } else {  // One process per worker.
    if (!CanCreateWorkerProcess(instance)) {
      queued_workers_.push_back(instance);
      return true;
    }
  }

  // Check to see if this shared worker is already running (two pages may have
  // tried to start up the worker simultaneously).
  if (instance.shared()) {
    // See if a worker with this name already exists.
    WorkerProcessHost::WorkerInstance* existing_instance =
        FindSharedWorkerInstance(
            instance.url(), instance.name(), instance.incognito());
    WorkerProcessHost::WorkerInstance::FilterInfo filter_info =
        instance.GetFilter();
    // If this worker is already running, no need to create a new copy. Just
    // inform the caller that the worker has been created.
    if (existing_instance) {
      // Walk the worker's filter list to see if this client is listed. If not,
      // then it means that the worker started by the client already exited so
      // we should not attach to this new one (http://crbug.com/29243).
      if (!existing_instance->HasFilter(filter_info.first, filter_info.second))
        return false;
      filter_info.first->Send(new ViewMsg_WorkerCreated(filter_info.second));
      return true;
    }

    // Look to see if there's a pending instance.
    WorkerProcessHost::WorkerInstance* pending = FindPendingInstance(
        instance.url(), instance.name(), instance.incognito());
    // If there's no instance *and* no pending instance (or there is a pending
    // instance but it does not contain our filter info), then it means the
    // worker started up and exited already. Log a warning because this should
    // be a very rare occurrence and is probably a bug, but it *can* happen so
    // handle it gracefully.
    if (!pending ||
        !pending->HasFilter(filter_info.first, filter_info.second)) {
      DLOG(WARNING) << "Pending worker already exited";
      return false;
    }

    // Assign the accumulated document set and filter list for this pending
    // worker to the new instance.
    DCHECK(!pending->worker_document_set()->IsEmpty());
    instance.ShareDocumentSet(*pending);
    for (WorkerProcessHost::WorkerInstance::FilterList::const_iterator i =
             pending->filters().begin();
         i != pending->filters().end(); ++i) {
      instance.AddFilter(i->first, i->second);
    }
    RemovePendingInstances(
        instance.url(), instance.name(), instance.incognito());

    // Remove any queued instances of this worker and copy over the filter to
    // this instance.
    for (WorkerProcessHost::Instances::iterator iter = queued_workers_.begin();
         iter != queued_workers_.end();) {
      if (iter->Matches(instance.url(), instance.name(),
                        instance.incognito())) {
        DCHECK(iter->NumFilters() == 1);
        WorkerProcessHost::WorkerInstance::FilterInfo filter_info =
            iter->GetFilter();
        instance.AddFilter(filter_info.first, filter_info.second);
        iter = queued_workers_.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  if (!worker) {
    WorkerMessageFilter* first_filter = instance.filters().begin()->first;
    worker = new WorkerProcessHost(
        first_filter->resource_dispatcher_host(),
        instance.request_context());
    // TODO(atwilson): This won't work if the message is from a worker process.
    // We don't support that yet though (this message is only sent from
    // renderers) but when we do, we'll need to add code to pass in the current
    // worker's document set for nested workers.
    if (!worker->Init(first_filter->render_process_id())) {
      delete worker;
      return false;
    }
  }

  // TODO(michaeln): As written, test can fail per my earlier comment in
  // this method, but that's a bug.
  // DCHECK(worker->request_context() == instance.request_context());

  worker->CreateWorker(instance);
  return true;
}

WorkerProcessHost* WorkerService::GetProcessForDomain(const GURL& url) {
  int num_processes = 0;
  std::string domain =
      net::RegistryControlledDomainService::GetDomainAndRegistry(url);
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    num_processes++;
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    for (WorkerProcessHost::Instances::const_iterator instance =
             worker->instances().begin();
         instance != worker->instances().end(); ++instance) {
      if (net::RegistryControlledDomainService::GetDomainAndRegistry(
              instance->url()) == domain) {
        return worker;
      }
    }
  }

  if (num_processes >= kMaxWorkerProcessesWhenSharing)
    return GetLeastLoadedWorker();

  return NULL;
}

WorkerProcessHost* WorkerService::GetProcessToFillUpCores() {
  int num_processes = 0;
  BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
  for (; !iter.Done(); ++iter)
    num_processes++;

  if (num_processes >= base::SysInfo::NumberOfProcessors())
    return GetLeastLoadedWorker();

  return NULL;
}

WorkerProcessHost* WorkerService::GetLeastLoadedWorker() {
  WorkerProcessHost* smallest = NULL;
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    if (!smallest || worker->instances().size() < smallest->instances().size())
      smallest = worker;
  }

  return smallest;
}

bool WorkerService::CanCreateWorkerProcess(
    const WorkerProcessHost::WorkerInstance& instance) {
  // Worker can be fired off if *any* parent has room.
  const WorkerDocumentSet::DocumentInfoSet& parents =
        instance.worker_document_set()->documents();

  for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
           parents.begin();
       parent_iter != parents.end(); ++parent_iter) {
    bool hit_total_worker_limit = false;
    if (TabCanCreateWorkerProcess(parent_iter->render_process_id(),
                                  parent_iter->render_view_id(),
                                  &hit_total_worker_limit)) {
      return true;
    }
    // Return false if already at the global worker limit (no need to continue
    // checking parent tabs).
    if (hit_total_worker_limit)
      return false;
  }
  // If we've reached here, none of the parent tabs is allowed to create an
  // instance.
  return false;
}

bool WorkerService::TabCanCreateWorkerProcess(int render_process_id,
                                              int render_view_id,
                                              bool* hit_total_worker_limit) {
  int total_workers = 0;
  int workers_per_tab = 0;
  *hit_total_worker_limit = false;
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    for (WorkerProcessHost::Instances::const_iterator cur_instance =
             worker->instances().begin();
         cur_instance != worker->instances().end(); ++cur_instance) {
      total_workers++;
      if (total_workers >= kMaxWorkersWhenSeparate) {
        *hit_total_worker_limit = true;
        return false;
      }
      if (cur_instance->RendererIsParent(render_process_id, render_view_id)) {
        workers_per_tab++;
        if (workers_per_tab >= kMaxWorkersPerTabWhenSeparate)
          return false;
      }
    }
  }

  return true;
}

void WorkerService::TryStartingQueuedWorker() {
  if (queued_workers_.empty())
    return;

  for (WorkerProcessHost::Instances::iterator i = queued_workers_.begin();
       i != queued_workers_.end();) {
    if (CanCreateWorkerProcess(*i)) {
      WorkerProcessHost::WorkerInstance instance = *i;
      queued_workers_.erase(i);
      CreateWorkerFromInstance(instance);

      // CreateWorkerFromInstance can modify the queued_workers_ list when it
      // coalesces queued instances after starting a shared worker, so we
      // have to rescan the list from the beginning (our iterator is now
      // invalid). This is not a big deal as having any queued workers will be
      // rare in practice so the list will be small.
      i = queued_workers_.begin();
    } else {
      ++i;
    }
  }
}

bool WorkerService::GetRendererForWorker(int worker_process_id,
                                         int* render_process_id,
                                         int* render_view_id) const {
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    if (iter->id() != worker_process_id)
        continue;

    // This code assumes one worker per process, see function comment in header!
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    WorkerProcessHost::Instances::const_iterator first_instance =
        worker->instances().begin();
    if (first_instance == worker->instances().end())
      return false;

    WorkerDocumentSet::DocumentInfoSet::const_iterator info =
        first_instance->worker_document_set()->documents().begin();
    *render_process_id = info->render_process_id();
    *render_view_id = info->render_view_id();
    return true;
  }
  return false;
}

const WorkerProcessHost::WorkerInstance* WorkerService::FindWorkerInstance(
      int worker_process_id) {
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    if (iter->id() != worker_process_id)
        continue;

    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    WorkerProcessHost::Instances::const_iterator instance =
        worker->instances().begin();
    return instance == worker->instances().end() ? NULL : &*instance;
  }
  return NULL;
}

WorkerProcessHost::WorkerInstance*
WorkerService::FindSharedWorkerInstance(const GURL& url, const string16& name,
                                        bool incognito) {
  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
       !iter.Done(); ++iter) {
    WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
    for (WorkerProcessHost::Instances::iterator instance_iter =
             worker->mutable_instances().begin();
         instance_iter != worker->mutable_instances().end();
         ++instance_iter) {
      if (instance_iter->Matches(url, name, incognito))
        return &(*instance_iter);
    }
  }
  return NULL;
}

WorkerProcessHost::WorkerInstance*
WorkerService::FindPendingInstance(const GURL& url, const string16& name,
                                   bool incognito) {
  // Walk the pending instances looking for a matching pending worker.
  for (WorkerProcessHost::Instances::iterator iter =
           pending_shared_workers_.begin();
       iter != pending_shared_workers_.end();
       ++iter) {
    if (iter->Matches(url, name, incognito)) {
      return &(*iter);
    }
  }
  return NULL;
}


void WorkerService::RemovePendingInstances(const GURL& url,
                                           const string16& name,
                                           bool incognito) {
  // Walk the pending instances looking for a matching pending worker.
  for (WorkerProcessHost::Instances::iterator iter =
           pending_shared_workers_.begin();
       iter != pending_shared_workers_.end(); ) {
    if (iter->Matches(url, name, incognito)) {
      iter = pending_shared_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

WorkerProcessHost::WorkerInstance*
WorkerService::CreatePendingInstance(const GURL& url,
                                     const string16& name,
                                     bool incognito) {
  // Look for an existing pending shared worker.
  WorkerProcessHost::WorkerInstance* instance =
      FindPendingInstance(url, name, incognito);
  if (instance)
    return instance;

  // No existing pending worker - create a new one.
  WorkerProcessHost::WorkerInstance pending(
      url, true, incognito, name, MSG_ROUTING_NONE, 0, 0, 0, NULL);
  pending_shared_workers_.push_back(pending);
  return &pending_shared_workers_.back();
}
