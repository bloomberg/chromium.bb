// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {
namespace {

void NotifyWorkerReadyForInspection(int worker_process_id,
                                    int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(NotifyWorkerReadyForInspection,
                                       worker_process_id,
                                       worker_route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyed(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyed, worker_process_id, worker_route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

}  // namespace

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance,
                                   SharedWorkerMessageFilter* filter,
                                   int worker_route_id)
    : instance_(instance),
      worker_document_set_(new WorkerDocumentSet()),
      worker_render_filter_(filter),
      worker_process_id_(filter->render_process_id()),
      worker_route_id_(worker_route_id),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(instance_);
  DCHECK(worker_render_filter_);
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_LONG_TIMES("SharedWorker.TimeToDeleted",
                           base::TimeTicks::Now() - creation_time_);
  if (!closed_ && !termination_message_sent_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
  SharedWorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
      worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::Start(bool pause_on_start) {
  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance_->url();
  params.name = instance_->name();
  params.content_security_policy = instance_->content_security_policy();
  params.security_policy_type = instance_->security_policy_type();
  params.creation_address_space = instance_->creation_address_space();
  params.pause_on_start = pause_on_start;
  params.route_id = worker_route_id_;
  Send(new WorkerProcessMsg_CreateWorker(params));

  for (const FilterInfo& info : filters_)
    info.filter()->Send(new ViewMsg_WorkerCreated(info.route_id()));
}

bool SharedWorkerHost::FilterConnectionMessage(
    int route_id,
    int sent_message_port_id,
    SharedWorkerMessageFilter* incoming_filter) {
  if (!IsAvailable() || !HasFilter(incoming_filter, route_id))
    return false;

  Connect(route_id, sent_message_port_id, incoming_filter);
  return true;
}

void SharedWorkerHost::FilterShutdown(SharedWorkerMessageFilter* filter) {
  RemoveFilters(filter);
  worker_document_set_->RemoveAll(filter);
  if (worker_document_set_->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    TerminateWorker();
  }
}

void SharedWorkerHost::DocumentDetached(SharedWorkerMessageFilter* filter,
                                        unsigned long long document_id) {
  // Walk all instances and remove the document from their document set.
  worker_document_set_->Remove(filter, document_id);
  if (worker_document_set_->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    TerminateWorker();
  }
}

void SharedWorkerHost::RenderFrameDetached(int render_process_id,
                                           int render_frame_id) {
  // Walk all instances and remove all the documents in the frame from their
  // document set.
  worker_document_set_->RemoveRenderFrame(render_process_id, render_frame_id);
  if (worker_document_set_->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    TerminateWorker();
  }
}

void SharedWorkerHost::WorkerContextClosed() {
  // Set the closed flag - this will stop any further messages from
  // being sent to the worker (messages can still be sent from the worker,
  // for exception reporting, etc).
  closed_ = true;
  if (!termination_message_sent_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::WorkerReadyForInspection() {
  NotifyWorkerReadyForInspection(worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::WorkerScriptLoaded() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoaded",
                      base::TimeTicks::Now() - creation_time_);
}

void SharedWorkerHost::WorkerScriptLoadFailed() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoadFailed",
                      base::TimeTicks::Now() - creation_time_);
  for (const FilterInfo& info : filters_)
    info.filter()->Send(new ViewMsg_WorkerScriptLoadFailed(info.route_id()));
}

void SharedWorkerHost::WorkerConnected(int message_port_id) {
  for (const FilterInfo& info : filters_) {
    if (info.message_port_id() != message_port_id)
      continue;
    info.filter()->Send(new ViewMsg_WorkerConnected(info.route_id()));
    return;
  }
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    std::unique_ptr<IPC::Message> reply_msg) {
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url,
      instance_->resource_context(),
      GetRenderFrameIDsForWorker(),
      base::Bind(&SharedWorkerHost::AllowFileSystemResponse,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&reply_msg)));
}

void SharedWorkerHost::AllowFileSystemResponse(
    std::unique_ptr<IPC::Message> reply_msg,
    bool allowed) {
  WorkerProcessHostMsg_RequestFileSystemAccessSync::WriteReplyParams(
      reply_msg.get(),
      allowed);
  Send(reply_msg.release());
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      const base::string16& name,
                                      bool* result) {
  *result = GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, instance_->resource_context(), GetRenderFrameIDsForWorker());
}

void SharedWorkerHost::TerminateWorker() {
  termination_message_sent_ = true;
  if (!closed_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
  Send(new WorkerMsg_TerminateWorkerContext(worker_route_id_));
}

std::vector<std::pair<int, int> >
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<std::pair<int, int> > result;
  const WorkerDocumentSet::DocumentInfoSet& documents =
      worker_document_set_->documents();
  for (const WorkerDocumentSet::DocumentInfo& doc : documents) {
    result.push_back(
        std::make_pair(doc.render_process_id(), doc.render_frame_id()));
  }
  return result;
}

bool SharedWorkerHost::IsAvailable() const {
  return !termination_message_sent_ && !closed_;
}

void SharedWorkerHost::AddFilter(SharedWorkerMessageFilter* filter,
                                 int route_id) {
  CHECK(filter);
  if (!HasFilter(filter, route_id)) {
    FilterInfo info(filter, route_id);
    filters_.push_back(info);
  }
}

void SharedWorkerHost::RemoveFilters(SharedWorkerMessageFilter* filter) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->filter() == filter)
      i = filters_.erase(i);
    else
      ++i;
  }
}

bool SharedWorkerHost::HasFilter(SharedWorkerMessageFilter* filter,
                                 int route_id) const {
  for (const FilterInfo& info : filters_) {
    if (info.filter() == filter && info.route_id() == route_id)
      return true;
  }
  return false;
}

void SharedWorkerHost::Connect(int route_id,
                               int sent_message_port_id,
                               SharedWorkerMessageFilter* incoming_filter) {
  DCHECK(IsAvailable());
  DCHECK(HasFilter(incoming_filter, route_id));
  DCHECK(worker_render_filter_);

  int new_routing_id = worker_render_filter_->GetNextRoutingID();
  MessagePortService::GetInstance()->UpdateMessagePort(
      sent_message_port_id,
      worker_render_filter_->message_port_message_filter(), new_routing_id);
  SetMessagePortID(incoming_filter, route_id, sent_message_port_id);
  Send(new WorkerMsg_Connect(worker_route_id_, sent_message_port_id,
                             new_routing_id));

  // Send any queued messages for the sent port.
  MessagePortService::GetInstance()->SendQueuedMessagesIfPossible(
      sent_message_port_id);
}

void SharedWorkerHost::SetMessagePortID(SharedWorkerMessageFilter* filter,
                                        int route_id,
                                        int message_port_id) {
  for (FilterInfo& info : filters_) {
    if (info.filter() == filter && info.route_id() == route_id) {
      info.set_message_port_id(message_port_id);
      return;
    }
  }
}

bool SharedWorkerHost::Send(IPC::Message* message) {
  return worker_render_filter_->Send(message);
}

}  // namespace content
