// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/message_port_service.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
namespace {

// Notifies RenderViewHost that one or more worker objects crashed.
void WorkerCrashCallback(int render_process_unique_id, int render_frame_id) {
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_unique_id, render_frame_id);
  if (host)
    host->delegate()->WorkerCrashed(host);
}

}  // namespace

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance)
    : instance_(instance),
      container_render_filter_(NULL),
      worker_route_id_(MSG_ROUTING_NONE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // If we crashed, tell the RenderViewHosts.
  if (instance_ && !instance_->load_failed()) {
    const WorkerDocumentSet::DocumentInfoSet& parents =
        instance_->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator parent_iter =
             parents.begin();
         parent_iter != parents.end();
         ++parent_iter) {
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&WorkerCrashCallback,
                                         parent_iter->render_process_id(),
                                         parent_iter->render_frame_id()));
    }
  }
}

bool SharedWorkerHost::Send(IPC::Message* message) {
  if (!container_render_filter_) {
    delete message;
    return false;
  }
  return container_render_filter_->Send(message);
}

void SharedWorkerHost::Init(SharedWorkerMessageFilter* filter) {
  CHECK(instance_);
  DCHECK(worker_route_id_ == MSG_ROUTING_NONE);
  container_render_filter_ = filter;
  worker_route_id_ = filter->GetNextRoutingID();

  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance_->url();
  params.name = instance_->name();
  params.content_security_policy = instance_->content_security_policy();
  params.security_policy_type = instance_->security_policy_type();
  params.route_id = worker_route_id_;
  Send(new WorkerProcessMsg_CreateWorker(params));

  for (SharedWorkerInstance::FilterList::const_iterator i =
           instance_->filters().begin();
       i != instance_->filters().end(); ++i) {
    i->filter()->Send(new ViewMsg_WorkerCreated(i->route_id()));
  }
}

bool SharedWorkerHost::FilterMessage(const IPC::Message& message,
                                     SharedWorkerMessageFilter* filter) {
  if (!instance_)
    return false;

  if (!instance_->closed() &&
      instance_->HasFilter(filter, message.routing_id())) {
    RelayMessage(message, filter);
    return true;
  }

  return false;
}

void SharedWorkerHost::FilterShutdown(SharedWorkerMessageFilter* filter) {
  if (!instance_)
    return;
  instance_->RemoveFilters(filter);
  instance_->worker_document_set()->RemoveAll(filter);
  if (instance_->worker_document_set()->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    Send(new WorkerMsg_TerminateWorkerContext(worker_route_id_));
  }
}

void SharedWorkerHost::DocumentDetached(SharedWorkerMessageFilter* filter,
                                        unsigned long long document_id) {
  if (!instance_)
    return;
  // Walk all instances and remove the document from their document set.
  instance_->worker_document_set()->Remove(filter, document_id);
  if (instance_->worker_document_set()->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    Send(new WorkerMsg_TerminateWorkerContext(worker_route_id_));
  }
}

void SharedWorkerHost::WorkerContextClosed() {
  if (!instance_)
    return;
  // Set the closed flag - this will stop any further messages from
  // being sent to the worker (messages can still be sent from the worker,
  // for exception reporting, etc).
  instance_->set_closed(true);
}

void SharedWorkerHost::WorkerContextDestroyed() {
  if (!instance_)
    return;
  instance_.reset();
}

void SharedWorkerHost::WorkerScriptLoaded() {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerHost::WorkerScriptLoadFailed() {
  if (!instance_)
    return;
  instance_->set_load_failed(true);
  for (SharedWorkerInstance::FilterList::const_iterator i =
           instance_->filters().begin();
       i != instance_->filters().end(); ++i) {
    i->filter()->Send(new ViewMsg_WorkerScriptLoadFailed(i->route_id()));
  }
}

void SharedWorkerHost::WorkerConnected(int message_port_id) {
  if (!instance_)
    return;
  for (SharedWorkerInstance::FilterList::const_iterator i =
           instance_->filters().begin();
       i != instance_->filters().end(); ++i) {
    if (i->message_port_id() != message_port_id)
      continue;
    i->filter()->Send(new ViewMsg_WorkerConnected(i->route_id()));
    return;
  }
}

void SharedWorkerHost::AllowDatabase(const GURL& url,
                                     const base::string16& name,
                                     const base::string16& display_name,
                                     unsigned long estimated_size,
                                     bool* result) {
  if (!instance_)
    return;
  *result = GetContentClient()->browser()->AllowWorkerDatabase(
      url,
      name,
      display_name,
      estimated_size,
      instance_->resource_context(),
      GetRenderFrameIDsForWorker());
}

void SharedWorkerHost::AllowFileSystem(const GURL& url,
                                       bool* result) {
  if (!instance_)
    return;
  *result = GetContentClient()->browser()->AllowWorkerFileSystem(
      url, instance_->resource_context(), GetRenderFrameIDsForWorker());
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      const base::string16& name,
                                      bool* result) {
  if (!instance_)
    return;
  *result = GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, instance_->resource_context(), GetRenderFrameIDsForWorker());
}

void SharedWorkerHost::RelayMessage(
    const IPC::Message& message,
    SharedWorkerMessageFilter* incoming_filter) {
  if (!instance_)
    return;
  if (message.type() == WorkerMsg_Connect::ID) {
    // Crack the SharedWorker Connect message to setup routing for the port.
    int sent_message_port_id;
    int new_routing_id;
    if (!WorkerMsg_Connect::Read(
            &message, &sent_message_port_id, &new_routing_id)) {
      return;
    }
    DCHECK(container_render_filter_);
    new_routing_id = container_render_filter_->GetNextRoutingID();
    MessagePortService::GetInstance()->UpdateMessagePort(
        sent_message_port_id,
        container_render_filter_->message_port_message_filter(),
        new_routing_id);
    instance_->SetMessagePortID(incoming_filter,
                                message.routing_id(),
                                sent_message_port_id);
    // Resend the message with the new routing id.
    Send(new WorkerMsg_Connect(
        worker_route_id_, sent_message_port_id, new_routing_id));

    // Send any queued messages for the sent port.
    MessagePortService::GetInstance()->SendQueuedMessagesIfPossible(
        sent_message_port_id);
  } else {
    IPC::Message* new_message = new IPC::Message(message);
    new_message->set_routing_id(worker_route_id_);
    Send(new_message);
    return;
  }
}

void SharedWorkerHost::TerminateWorker() {
  Send(new WorkerMsg_TerminateWorkerContext(worker_route_id_));
}

std::vector<std::pair<int, int> >
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<std::pair<int, int> > result;
  if (!instance_)
    return result;
  const WorkerDocumentSet::DocumentInfoSet& documents =
      instance_->worker_document_set()->documents();
  for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
           documents.begin();
       doc != documents.end();
       ++doc) {
    result.push_back(
        std::make_pair(doc->render_process_id(), doc->render_frame_id()));
  }
  return result;
}

}  // namespace content
