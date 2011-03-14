// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webworker_base.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "content/common/child_thread.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerClient.h"

using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebWorkerClient;

WebWorkerBase::WebWorkerBase(
    ChildThread* child_thread,
    unsigned long long document_id,
    int route_id,
    int render_view_route_id,
    int parent_appcache_host_id)
    : route_id_(route_id),
      render_view_route_id_(render_view_route_id),
      child_thread_(child_thread),
      document_id_(document_id),
      parent_appcache_host_id_(parent_appcache_host_id) {
  if (route_id_ != MSG_ROUTING_NONE)
    child_thread_->AddRoute(route_id_, this);
}

WebWorkerBase::~WebWorkerBase() {
  Disconnect();

  // Free up any unsent queued messages.
  for (size_t i = 0; i < queued_messages_.size(); ++i)
    delete queued_messages_[i];
}

void WebWorkerBase::Disconnect() {
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  // So the messages from WorkerContext (like WorkerContextDestroyed) do not
  // come after nobody is listening. Since Worker and WorkerContext can
  // terminate independently, already sent messages may still be in the pipe.
  child_thread_->RemoveRoute(route_id_);

  route_id_ = MSG_ROUTING_NONE;
}

void WebWorkerBase::CreateWorkerContext(const GURL& script_url,
                                        bool is_shared,
                                        const string16& name,
                                        const string16& user_agent,
                                        const string16& source_code,
                                        int pending_route_id,
                                        int64 script_resource_appcache_id) {
  DCHECK(route_id_ == MSG_ROUTING_NONE);
  ViewHostMsg_CreateWorker_Params params;
  params.url = script_url;
  params.is_shared = is_shared;
  params.name = name;
  params.document_id = document_id_;
  params.render_view_route_id = render_view_route_id_;
  params.route_id = pending_route_id;
  params.parent_appcache_host_id = parent_appcache_host_id_;
  params.script_resource_appcache_id = script_resource_appcache_id;
  IPC::Message* create_message = new ViewHostMsg_CreateWorker(
      params, &route_id_);
  child_thread_->Send(create_message);
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  child_thread_->AddRoute(route_id_, this);

  // We make sure that the start message is the first, since postMessage or
  // connect might have already been called.
  queued_messages_.insert(queued_messages_.begin(),
      new WorkerMsg_StartWorkerContext(
          route_id_, script_url, user_agent, source_code));
}

bool WebWorkerBase::IsStarted() {
  // Worker is started if we have a route ID and there are no queued messages
  // (meaning we've sent the WorkerMsg_StartWorkerContext already).
  return (route_id_ != MSG_ROUTING_NONE && queued_messages_.empty());
}

bool WebWorkerBase::Send(IPC::Message* message) {
  // It's possible that messages will be sent before the worker is created, in
  // which case route_id_ will be none.  Or the worker object can be interacted
  // with before the browser process told us that it started, in which case we
  // also want to queue the message.
  if (!IsStarted()) {
    queued_messages_.push_back(message);
    return true;
  }

  // For now we proxy all messages to the worker process through the browser.
  // Revisit if we find this slow.
  // TODO(jabdelmalek): handle sync messages if we need them.
  IPC::Message* wrapped_msg = new ViewHostMsg_ForwardToWorker(*message);
  delete message;
  return child_thread_->Send(wrapped_msg);
}

void WebWorkerBase::SendQueuedMessages() {
  DCHECK(queued_messages_.size());
  std::vector<IPC::Message*> queued_messages = queued_messages_;
  queued_messages_.clear();
  for (size_t i = 0; i < queued_messages.size(); ++i) {
    queued_messages[i]->set_routing_id(route_id_);
    Send(queued_messages[i]);
  }
}
