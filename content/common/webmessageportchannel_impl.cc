// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webmessageportchannel_impl.h"

#include "base/bind.h"
#include "content/common/child_process.h"
#include "content/common/child_thread.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMessagePortChannelClient.h"

using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
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
  // If we have any queued messages with attached ports, manually destroy them.
  while (!message_queue_.empty()) {
    const std::vector<WebMessagePortChannelImpl*>& channel_array =
        message_queue_.front().ports;
    for (size_t i = 0; i < channel_array.size(); i++) {
      channel_array[i]->destroy();
    }
    message_queue_.pop();
  }

  if (message_port_id_ != MSG_ROUTING_NONE)
    Send(new WorkerProcessHostMsg_DestroyMessagePort(message_port_id_));

  if (route_id_ != MSG_ROUTING_NONE)
    ChildThread::current()->RemoveRoute(route_id_);
}

void WebMessagePortChannelImpl::setClient(WebMessagePortChannelClient* client) {
  // Must lock here since client_ is called on the main thread.
  base::AutoLock auto_lock(lock_);
  client_ = client;
}

void WebMessagePortChannelImpl::destroy() {
  setClient(NULL);

  // Release the object on the main thread, since the destructor might want to
  // send an IPC, and that has to happen on the main thread.
  ChildThread::current()->message_loop()->ReleaseSoon(FROM_HERE, this);
}

void WebMessagePortChannelImpl::entangle(WebMessagePortChannel* channel) {
  // The message port ids might not be set up yet, if this channel wasn't
  // created on the main thread.  So need to wait until we're on the main thread
  // before getting the other message port id.
  scoped_refptr<WebMessagePortChannelImpl> webchannel(
      static_cast<WebMessagePortChannelImpl*>(channel));
  Entangle(webchannel);
}

void WebMessagePortChannelImpl::postMessage(
    const WebString& message,
    WebMessagePortChannelArray* channels) {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    ChildThread::current()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::postMessage, this,
                   message, channels));
    return;
  }

  std::vector<int> message_port_ids(channels ? channels->size() : 0);
  if (channels) {
    // Extract the port IDs from the source array, then free it.
    for (size_t i = 0; i < channels->size(); ++i) {
      WebMessagePortChannelImpl* webchannel =
          static_cast<WebMessagePortChannelImpl*>((*channels)[i]);
      message_port_ids[i] = webchannel->message_port_id();
      webchannel->QueueMessages();
      DCHECK(message_port_ids[i] != MSG_ROUTING_NONE);
    }
    delete channels;
  }

  IPC::Message* msg = new WorkerProcessHostMsg_PostMessage(
      message_port_id_, message, message_port_ids);
  Send(msg);
}

bool WebMessagePortChannelImpl::tryGetMessage(
    WebString* message,
    WebMessagePortChannelArray& channels) {
  base::AutoLock auto_lock(lock_);
  if (message_queue_.empty())
    return false;

  *message = message_queue_.front().message;
  const std::vector<WebMessagePortChannelImpl*>& channel_array =
      message_queue_.front().ports;
  WebMessagePortChannelArray result_ports(channel_array.size());
  for (size_t i = 0; i < channel_array.size(); i++) {
    result_ports[i] = channel_array[i];
  }

  channels.swap(result_ports);
  message_queue_.pop();
  return true;
}

void WebMessagePortChannelImpl::Init() {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    ChildThread::current()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Init, this));
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
    ChildThread::current()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Entangle, this, channel));
    return;
  }

  Send(new WorkerProcessHostMsg_Entangle(
      message_port_id_, channel->message_port_id()));
}

void WebMessagePortChannelImpl::QueueMessages() {
  if (MessageLoop::current() != ChildThread::current()->message_loop()) {
    ChildThread::current()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::QueueMessages, this));
    return;
  }
  // This message port is being sent elsewhere (perhaps to another process).
  // The new endpoint needs to receive the queued messages, including ones that
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
    ChildThread::current()->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&WebMessagePortChannelImpl::Send, this, message));
    return;
  }

  ChildThread::current()->Send(message);
}

bool WebMessagePortChannelImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebMessagePortChannelImpl, message)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_Message, OnMessage)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_MessagesQueued, OnMessagedQueued)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebMessagePortChannelImpl::OnMessage(
    const string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  base::AutoLock auto_lock(lock_);
  Message msg;
  msg.message = message;
  if (!sent_message_port_ids.empty()) {
    msg.ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      msg.ports[i] = new WebMessagePortChannelImpl(
          new_routing_ids[i], sent_message_port_ids[i]);
    }
  }

  bool was_empty = message_queue_.empty();
  message_queue_.push(msg);
  if (client_ && was_empty)
    client_->messageAvailable();
}

void WebMessagePortChannelImpl::OnMessagedQueued() {
  std::vector<QueuedMessage> queued_messages;

  {
    base::AutoLock auto_lock(lock_);
    queued_messages.reserve(message_queue_.size());
    while (!message_queue_.empty()) {
      string16 message = message_queue_.front().message;
      const std::vector<WebMessagePortChannelImpl*>& channel_array =
          message_queue_.front().ports;
      std::vector<int> port_ids(channel_array.size());
      for (size_t i = 0; i < channel_array.size(); ++i) {
        port_ids[i] = channel_array[i]->message_port_id();
      }
      queued_messages.push_back(std::make_pair(message, port_ids));
      message_queue_.pop();
    }
  }

  Send(new WorkerProcessHostMsg_SendQueuedMessages(
      message_port_id_, queued_messages));

  message_port_id_ = MSG_ROUTING_NONE;

  Release();
  ChildProcess::current()->ReleaseProcess();
}

WebMessagePortChannelImpl::Message::Message() {}

WebMessagePortChannelImpl::Message::~Message() {}
