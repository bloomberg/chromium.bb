// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/websharedworker_stub.h"

#include "content/common/child_thread.h"
#include "content/common/file_system/file_system_dispatcher.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSharedWorker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

WebSharedWorkerStub::WebSharedWorkerStub(
    const string16& name, int route_id,
    const WorkerAppCacheInitInfo& appcache_init_info)
    : WebWorkerStubBase(route_id, appcache_init_info),
      name_(name),
      started_(false) {
  // TODO(atwilson): Add support for NaCl when they support MessagePorts.
  impl_ = WebKit::WebSharedWorker::create(client());
}

WebSharedWorkerStub::~WebSharedWorkerStub() {
  impl_->clientDestroyed();
}

bool WebSharedWorkerStub::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerStub, message)
    IPC_MESSAGE_HANDLER(WorkerMsg_StartWorkerContext, OnStartWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSharedWorkerStub::OnChannelError() {
    OnTerminateWorkerContext();
}

const GURL& WebSharedWorkerStub::url() const {
  return url_;
}

void WebSharedWorkerStub::OnStartWorkerContext(
    const GURL& url, const string16& user_agent, const string16& source_code) {
  // Ignore multiple attempts to start this worker (can happen if two pages
  // try to start it simultaneously).
  if (started_)
    return;

  impl_->startWorkerContext(url, name_, user_agent, source_code, 0);
  started_ = true;
  url_ = url;

  // Process any pending connections.
  for (PendingConnectInfoList::const_iterator iter = pending_connects_.begin();
       iter != pending_connects_.end();
       ++iter) {
    OnConnect(iter->first, iter->second);
  }
  pending_connects_.clear();
}

void WebSharedWorkerStub::OnConnect(int sent_message_port_id, int routing_id) {
  if (started_) {
    WebKit::WebMessagePortChannel* channel =
        new WebMessagePortChannelImpl(routing_id, sent_message_port_id);
    impl_->connect(channel, NULL);
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // WorkerMsg_Connect for one of the documents can come in before the
    // worker is started. Just queue up the connect and deliver it once the
    // worker starts.
    PendingConnectInfo pending_connect(sent_message_port_id, routing_id);
    pending_connects_.push_back(pending_connect);
  }
}

void WebSharedWorkerStub::OnTerminateWorkerContext() {
  impl_->terminateWorkerContext();

  // Call the client to make sure context exits.
  EnsureWorkerContextTerminates();
  started_ = false;
}
