// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webmessageportchannel_impl.h"

#include "chrome/common/child_process.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/worker_messages.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebMessagePortChannelClient.h"

using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelClient;
using WebKit::WebString;

WebMessagePortChannelImpl::WebMessagePortChannelImpl()
    : client_(NULL),
      route_id_(MSG_ROUTING_NONE),
      message_port_id_(MSG_ROUTING_NONE) {
  AddRef();
  Init();
}

WebMessagePortChannelImpl::WebMessagePortChannelImpl(
    int route_id,
    int message_port_id)
    : client_(NULL),
      route_id_(route_id),
      message_port_id_(message_port_id) {
  AddRef();
  Init();
}

WebMessagePortChannelImpl::~WebMessagePortChannelImpl() {
  if (message_port_id_ != MSG_ROUTING_NONE)
    Send(new WorkerProcessHostMsg_DestroyMessagePort(message_port_id_));

  if (route_id_ != MSG_ROUTING_NONE)
    ChildThread::current()->RemoveRoute(route_id_);
}

void WebMessagePortChannelImpl::setClient(WebMessagePortChannelClient* client) {
  // Must lock here since client_ is called on the main thread.
  AutoLock auto_lock(lock_);
  client_ = client;
}

void WebMessagePortChannelImpl::destroy() {
  setClient(NULL);
  Release();
}

void WebMessagePortChannelImpl::entangle(WebMessagePortChannel* channel) {
  // The message port ids might not be set up yet, if this channel wasn't
  // created on the main thread.  So need to wait until we're on the main thread
  // before getting the other message port id.
  scoped_refptr<WebMessagePortChannelImpl> webchannel =
      static_cast<WebMessagePortChannelImpl*>(channel);
  Entangle(webchannel);
}

void WebMessagePortChannelImpl::postMessage(
    const WebString& message,
    WebMessagePortChannel* channel) {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    ChildThread::current()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebMessagePortChannelImpl::postMessage,
            message, channel));
    return;
  }

  WebMessagePortChannelImpl* webchannel =
      static_cast<WebMessagePortChannelImpl*>(channel);

  int message_port_id = MSG_ROUTING_NONE;
  if (webchannel) {
    message_port_id = webchannel->message_port_id();
    webchannel->QueueMessages();
    DCHECK(message_port_id != MSG_ROUTING_NONE);
  }

  IPC::Message* msg = new WorkerProcessHostMsg_PostMessage(
      message_port_id_, message, message_port_id);

  Send(msg);
}

bool WebMessagePortChannelImpl::tryGetMessage(
    WebString* message,
    WebMessagePortChannel** channel) {
  AutoLock auto_lock(lock_);
  if (message_queue_.empty())
    return false;

  *message = message_queue_.front().message;
  *channel = message_queue_.front().port.release();
  message_queue_.pop();
  return true;
}

void WebMessagePortChannelImpl::Init() {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    ChildThread::current()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebMessagePortChannelImpl::Init));
    return;
  }

  if (route_id_ == MSG_ROUTING_NONE) {
    DCHECK(message_port_id_ == MSG_ROUTING_NONE);
    Send(new WorkerProcessHostMsg_CreateMessagePort(
        &route_id_, &message_port_id_));
  }

  ChildThread::current()->AddRoute(route_id_, this);
}

void WebMessagePortChannelImpl::Entangle(
    scoped_refptr<WebMessagePortChannelImpl> channel) {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    ChildThread::current()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebMessagePortChannelImpl::Entangle, channel));
    return;
  }

  Send(new WorkerProcessHostMsg_Entangle(
      message_port_id_, channel->message_port_id()));
}

void WebMessagePortChannelImpl::QueueMessages() {
  // This message port is being sent elsewhere (perhaps to another process).
  // The new endpoint needs to recieve the queued messages, including ones that
  // could still be in-flight.  So we tell the browser to queue messages, and it
  // sends us an ack, whose receipt we know means that no more messages are
  // in-flight.  We then send the queued messages to the browser, which prepends
  // them to the ones it queued and it sends them to the new endpoint.
  Send(new WorkerProcessHostMsg_QueueMessages(message_port_id_));

  // The process could potentially go away while we're still waiting for
  // in-flight messages.  Ensure it stays alive.
  ChildProcess::current()->AddRefProcess();
}

void WebMessagePortChannelImpl::Send(IPC::Message* message) {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    DCHECK(!message->is_sync());
    ChildThread::current()->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &WebMessagePortChannelImpl::Send, message));
    return;
  }

  ChildThread::current()->Send(message);
}

void WebMessagePortChannelImpl::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(WebMessagePortChannelImpl, message)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_Message, OnMessage)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_MessagesQueued, OnMessagedQueued)
  IPC_END_MESSAGE_MAP()
}

void WebMessagePortChannelImpl::OnMessage(const string16& message,
                                          int sent_message_port_id,
                                          int new_routing_id) {
  AutoLock auto_lock(lock_);
  Message msg;
  msg.message = message;
  msg.port = NULL;
  if (sent_message_port_id != MSG_ROUTING_NONE) {
    msg.port = new WebMessagePortChannelImpl(
        new_routing_id, sent_message_port_id);
  }

  bool was_empty = message_queue_.empty();
  message_queue_.push(msg);
  if (client_ && was_empty)
    client_->messageAvailable();
}

void WebMessagePortChannelImpl::OnMessagedQueued() {
  std::vector<std::pair<string16, int> > queued_messages;

  {
    AutoLock auto_lock(lock_);
    queued_messages.reserve(message_queue_.size());
    while (!message_queue_.empty()) {
      string16 message = message_queue_.front().message;
      int port = MSG_ROUTING_NONE;
      if (message_queue_.front().port)
        port = message_queue_.front().port->message_port_id();

      queued_messages.push_back(std::make_pair(message, port));
      message_queue_.pop();
    }
  }

  Send(new WorkerProcessHostMsg_SendQueuedMessages(
      message_port_id_, queued_messages));

  message_port_id_ = MSG_ROUTING_NONE;

  Release();
  ChildProcess::current()->ReleaseProcess();
}
