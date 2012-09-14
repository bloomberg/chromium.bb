// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_service_impl.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "content/browser/debugger/worker_devtools_manager.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"

namespace content {

const int WorkerServiceImpl::kMaxWorkersWhenSeparate = 64;
const int WorkerServiceImpl::kMaxWorkersPerTabWhenSeparate = 16;

WorkerService* WorkerService::GetInstance() {
  return WorkerServiceImpl::GetInstance();
}

WorkerServiceImpl* WorkerServiceImpl::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return Singleton<WorkerServiceImpl>::get();
}

WorkerServiceImpl::WorkerServiceImpl() : next_worker_route_id_(0) {
}

WorkerServiceImpl::~WorkerServiceImpl() {
  // The observers in observers_ can't be used here because they might be
  // gone already.
}

void WorkerServiceImpl::OnWorkerMessageFilterClosing(
    WorkerMessageFilter* filter) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    iter->FilterShutdown(filter);
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

  for (WorkerProcessHost::Instances::iterator i =
           pending_shared_workers_.begin();
       i != pending_shared_workers_.end(); ) {
     i->RemoveFilters(filter);
     if (i->NumFilters() == 0) {
      i = pending_shared_workers_.erase(i);
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

void WorkerServiceImpl::CreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int route_id,
    WorkerMessageFilter* filter,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Generate a unique route id for the browser-worker communication that's
  // unique among all worker processes.  That way when the worker process sends
  // a wrapped IPC message through us, we know which WorkerProcessHost to give
  // it to.
  WorkerProcessHost::WorkerInstance instance(
      params.url,
      params.name,
      next_worker_route_id(),
      0,
      params.script_resource_appcache_id,
      resource_context,
      partition);
  instance.AddFilter(filter, route_id);
  instance.worker_document_set()->Add(
      filter, params.document_id, filter->render_process_id(),
      params.render_view_route_id);

  CreateWorkerFromInstance(instance);
}

void WorkerServiceImpl::LookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int route_id,
    WorkerMessageFilter* filter,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    bool* exists,
    bool* url_mismatch) {
  *exists = true;
  WorkerProcessHost::WorkerInstance* instance = FindSharedWorkerInstance(
      params.url, params.name, partition, resource_context);

  if (!instance) {
    // If no worker instance currently exists, we need to create a pending
    // instance - this is to make sure that any subsequent lookups passing a
    // mismatched URL get the appropriate url_mismatch error at lookup time.
    // Having named shared workers was a Really Bad Idea due to details like
    // this.
    instance = CreatePendingInstance(params.url, params.name,
                                     resource_context, partition);
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

void WorkerServiceImpl::CancelCreateDedicatedWorker(
    int route_id,
    WorkerMessageFilter* filter) {

  NOTREACHED();
}

void WorkerServiceImpl::ForwardToWorker(const IPC::Message& message,
                                        WorkerMessageFilter* filter) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter->FilterMessage(message, filter))
      return;
  }

  // TODO(jabdelmalek): tell filter that callee is gone
}

void WorkerServiceImpl::DocumentDetached(unsigned long long document_id,
                                         WorkerMessageFilter* filter) {
  // Any associated shared workers can be shut down.
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter)
    iter->DocumentDetached(filter, document_id);

  // Remove any queued shared workers for this document.
  for (WorkerProcessHost::Instances::iterator iter = queued_workers_.begin();
       iter != queued_workers_.end();) {

    iter->worker_document_set()->Remove(filter, document_id);
    if (iter->worker_document_set()->IsEmpty()) {
      iter = queued_workers_.erase(iter);
      continue;
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

bool WorkerServiceImpl::CreateWorkerFromInstance(
    WorkerProcessHost::WorkerInstance instance) {
  if (!CanCreateWorkerProcess(instance)) {
    queued_workers_.push_back(instance);
    return true;
  }

  // Check to see if this shared worker is already running (two pages may have
  // tried to start up the worker simultaneously).
  // See if a worker with this name already exists.
  WorkerProcessHost::WorkerInstance* existing_instance =
      FindSharedWorkerInstance(
          instance.url(), instance.name(), instance.partition(),
          instance.resource_context());
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
      instance.url(), instance.name(), instance.partition(),
      instance.resource_context());
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
  RemovePendingInstances(instance.url(), instance.name(),
                         instance.partition(), instance.resource_context());

  // Remove any queued instances of this worker and copy over the filter to
  // this instance.
  for (WorkerProcessHost::Instances::iterator iter = queued_workers_.begin();
       iter != queued_workers_.end();) {
    if (iter->Matches(instance.url(), instance.name(),
                      instance.partition(), instance.resource_context())) {
      DCHECK(iter->NumFilters() == 1);
      WorkerProcessHost::WorkerInstance::FilterInfo filter_info =
          iter->GetFilter();
      instance.AddFilter(filter_info.first, filter_info.second);
      iter = queued_workers_.erase(iter);
    } else {
      ++iter;
    }
  }

  WorkerMessageFilter* first_filter = instance.filters().begin()->first;
  WorkerProcessHost* worker = new WorkerProcessHost(
      instance.resource_context(), instance.partition());
  // TODO(atwilson): This won't work if the message is from a worker process.
  // We don't support that yet though (this message is only sent from
  // renderers) but when we do, we'll need to add code to pass in the current
  // worker's document set for nested workers.
  if (!worker->Init(first_filter->render_process_id())) {
    delete worker;
    return false;
  }

  worker->CreateWorker(instance);
  FOR_EACH_OBSERVER(
      WorkerServiceObserver, observers_,
      WorkerCreated(instance.url(), instance.name(), worker->GetData().id,
                    instance.worker_route_id()));
  WorkerDevToolsManager::GetInstance()->WorkerCreated(worker, instance);
  return true;
}

bool WorkerServiceImpl::CanCreateWorkerProcess(
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

bool WorkerServiceImpl::TabCanCreateWorkerProcess(
    int render_process_id,
    int render_view_id,
    bool* hit_total_worker_limit) {
  int total_workers = 0;
  int workers_per_tab = 0;
  *hit_total_worker_limit = false;
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    for (WorkerProcessHost::Instances::const_iterator cur_instance =
             iter->instances().begin();
         cur_instance != iter->instances().end(); ++cur_instance) {
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

void WorkerServiceImpl::TryStartingQueuedWorker() {
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

bool WorkerServiceImpl::GetRendererForWorker(int worker_process_id,
                                             int* render_process_id,
                                             int* render_view_id) const {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id != worker_process_id)
      continue;

    // This code assumes one worker per process, see function comment in header!
    WorkerProcessHost::Instances::const_iterator first_instance =
        iter->instances().begin();
    if (first_instance == iter->instances().end())
      return false;

    WorkerDocumentSet::DocumentInfoSet::const_iterator info =
        first_instance->worker_document_set()->documents().begin();
    *render_process_id = info->render_process_id();
    *render_view_id = info->render_view_id();
    return true;
  }
  return false;
}

const WorkerProcessHost::WorkerInstance* WorkerServiceImpl::FindWorkerInstance(
      int worker_process_id) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id != worker_process_id)
        continue;

    WorkerProcessHost::Instances::const_iterator instance =
        iter->instances().begin();
    return instance == iter->instances().end() ? NULL : &*instance;
  }
  return NULL;
}

bool WorkerServiceImpl::TerminateWorker(int process_id, int route_id) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().id == process_id) {
      iter->TerminateWorker(route_id);
      return true;
    }
  }
  return false;
}

std::vector<WorkerService::WorkerInfo> WorkerServiceImpl::GetWorkers() {
  std::vector<WorkerService::WorkerInfo> results;
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    const WorkerProcessHost::Instances& instances = (*iter)->instances();
    for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
         i != instances.end(); ++i) {
      WorkerService::WorkerInfo info;
      info.url = i->url();
      info.name = i->name();
      info.route_id = i->worker_route_id();
      info.process_id = iter.GetData().id;
      info.handle = iter.GetData().handle;
      results.push_back(info);
    }
  }
  return results;
}

void WorkerServiceImpl::AddObserver(WorkerServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.AddObserver(observer);
}

void WorkerServiceImpl::RemoveObserver(WorkerServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  observers_.RemoveObserver(observer);
}

void WorkerServiceImpl::NotifyWorkerDestroyed(
    WorkerProcessHost* process,
    int worker_route_id) {
  WorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      process, worker_route_id);
  FOR_EACH_OBSERVER(WorkerServiceObserver, observers_,
                    WorkerDestroyed(process->GetData().id, worker_route_id));
}

WorkerProcessHost::WorkerInstance* WorkerServiceImpl::FindSharedWorkerInstance(
    const GURL& url,
    const string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  for (WorkerProcessHostIterator iter; !iter.Done(); ++iter) {
    for (WorkerProcessHost::Instances::iterator instance_iter =
             iter->mutable_instances().begin();
         instance_iter != iter->mutable_instances().end();
         ++instance_iter) {
      if (instance_iter->Matches(url, name, partition, resource_context))
        return &(*instance_iter);
    }
  }
  return NULL;
}

WorkerProcessHost::WorkerInstance* WorkerServiceImpl::FindPendingInstance(
    const GURL& url,
    const string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  // Walk the pending instances looking for a matching pending worker.
  for (WorkerProcessHost::Instances::iterator iter =
           pending_shared_workers_.begin();
       iter != pending_shared_workers_.end();
       ++iter) {
    if (iter->Matches(url, name, partition, resource_context))
      return &(*iter);
  }
  return NULL;
}


void WorkerServiceImpl::RemovePendingInstances(
    const GURL& url,
    const string16& name,
    const WorkerStoragePartition& partition,
    ResourceContext* resource_context) {
  // Walk the pending instances looking for a matching pending worker.
  for (WorkerProcessHost::Instances::iterator iter =
           pending_shared_workers_.begin();
       iter != pending_shared_workers_.end(); ) {
    if (iter->Matches(url, name, partition, resource_context)) {
      iter = pending_shared_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

WorkerProcessHost::WorkerInstance* WorkerServiceImpl::CreatePendingInstance(
    const GURL& url,
    const string16& name,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition) {
  // Look for an existing pending shared worker.
  WorkerProcessHost::WorkerInstance* instance =
      FindPendingInstance(url, name, partition, resource_context);
  if (instance)
    return instance;

  // No existing pending worker - create a new one.
  WorkerProcessHost::WorkerInstance pending(
      url, true, name, resource_context, partition);
  pending_shared_workers_.push_back(pending);
  return &pending_shared_workers_.back();
}

}  // namespace content
