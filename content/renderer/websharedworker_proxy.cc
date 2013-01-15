// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/websharedworker_proxy.h"
#include "content/common/child_thread.h"
#include "content/common/view_messages.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSharedWorkerClient.h"

namespace content {

WebSharedWorkerProxy::WebSharedWorkerProxy(ChildThread* child_thread,
                                           unsigned long long document_id,
                                           bool exists,
                                           int route_id,
                                           int render_view_route_id)
    : route_id_(exists ? route_id : MSG_ROUTING_NONE),
      render_view_route_id_(render_view_route_id),
      child_thread_(child_thread),
      document_id_(document_id),
      pending_route_id_(route_id),
      connect_listener_(NULL) {
  if (route_id_ != MSG_ROUTING_NONE)
    child_thread_->AddRoute(route_id_, this);
}

WebSharedWorkerProxy::~WebSharedWorkerProxy() {
  Disconnect();

  // Free up any unsent queued messages.
  for (size_t i = 0; i < queued_messages_.size(); ++i)
    delete queued_messages_[i];
}

void WebSharedWorkerProxy::Disconnect() {
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  // So the messages from WorkerContext (like WorkerContextDestroyed) do not
  // come after nobody is listening. Since Worker and WorkerContext can
  // terminate independently, already sent messages may still be in the pipe.
  child_thread_->RemoveRoute(route_id_);

  route_id_ = MSG_ROUTING_NONE;
}

void WebSharedWorkerProxy::CreateWorkerContext(
    const GURL& script_url,
    bool is_shared,
    const string16& name,
    const string16& user_agent,
    const string16& source_code,
    const string16& content_security_policy,
    WebKit::WebContentSecurityPolicyType policy_type,
    int pending_route_id,
    int64 script_resource_appcache_id) {
  DCHECK(route_id_ == MSG_ROUTING_NONE);
  ViewHostMsg_CreateWorker_Params params;
  params.url = script_url;
  params.name = name;
  params.document_id = document_id_;
  params.render_view_route_id = render_view_route_id_;
  params.route_id = pending_route_id;
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
          route_id_, script_url, user_agent, source_code,
          content_security_policy, policy_type));
}

bool WebSharedWorkerProxy::IsStarted() {
  // Worker is started if we have a route ID and there are no queued messages
  // (meaning we've sent the WorkerMsg_StartWorkerContext already).
  return (route_id_ != MSG_ROUTING_NONE && queued_messages_.empty());
}

bool WebSharedWorkerProxy::Send(IPC::Message* message) {
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

void WebSharedWorkerProxy::SendQueuedMessages() {
  DCHECK(queued_messages_.size());
  std::vector<IPC::Message*> queued_messages = queued_messages_;
  queued_messages_.clear();
  for (size_t i = 0; i < queued_messages.size(); ++i) {
    queued_messages[i]->set_routing_id(route_id_);
    Send(queued_messages[i]);
  }
}

bool WebSharedWorkerProxy::isStarted() {
  return IsStarted();
}

void WebSharedWorkerProxy::startWorkerContext(
    const WebKit::WebURL& script_url,
    const WebKit::WebString& name,
    const WebKit::WebString& user_agent,
    const WebKit::WebString& source_code,
    const WebKit::WebString& content_security_policy,
    WebKit::WebContentSecurityPolicyType policy_type,
    long long script_resource_appcache_id) {
  DCHECK(!isStarted());
  CreateWorkerContext(
      script_url, true, name, user_agent, source_code, content_security_policy,
      policy_type, pending_route_id_, script_resource_appcache_id);
}

void WebSharedWorkerProxy::terminateWorkerContext() {
  // This API should only be invoked from worker context.
  NOTREACHED();
}

void WebSharedWorkerProxy::clientDestroyed() {
  // This API should only be invoked from worker context.
  NOTREACHED();
}

void WebSharedWorkerProxy::connect(WebKit::WebMessagePortChannel* channel,
                                   ConnectListener* listener) {
  WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channel);

  int message_port_id = webchannel->message_port_id();
  DCHECK(message_port_id != MSG_ROUTING_NONE);
  webchannel->QueueMessages();

  Send(new WorkerMsg_Connect(route_id_, message_port_id, MSG_ROUTING_NONE));
  if (HasQueuedMessages()) {
    connect_listener_ = listener;
  } else {
    listener->connected();
    // The listener may free this object, so do not access the object after
    // this point.
  }
}

bool WebSharedWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerProxy, message)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerCreated, OnWorkerCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSharedWorkerProxy::OnWorkerCreated() {
  // The worker is created - now send off the CreateWorkerContext message and
  // any other queued messages
  SendQueuedMessages();

  // Inform any listener that the pending connect event has been sent
  // (this can result in this object being freed).
  if (connect_listener_) {
    connect_listener_->connected();
  }
}

}  // namespace content
