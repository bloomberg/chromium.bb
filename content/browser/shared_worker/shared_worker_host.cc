// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include "content/browser/message_port_service.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance)
    : instance_(instance),
      container_render_filter_(NULL),
      worker_route_id_(MSG_ROUTING_NONE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerHost::AllowFileSystem(const GURL& url,
                                       bool* result) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      const base::string16& name,
                                      bool* result) {
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
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

}  // namespace content
