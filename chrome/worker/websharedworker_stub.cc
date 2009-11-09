// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/websharedworker_stub.h"

#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "webkit/api/public/WebSharedWorker.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"

WebSharedWorkerStub::WebSharedWorkerStub(
    const string16& name, int route_id)
    : WebWorkerStubBase(route_id),
      name_(name) {

  // TODO(atwilson): Add support for NaCl when they support MessagePorts.
  impl_ = WebKit::WebSharedWorker::create(client());

}

WebSharedWorkerStub::~WebSharedWorkerStub() {
  impl_->clientDestroyed();
}

void WebSharedWorkerStub::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerStub, message)
    IPC_MESSAGE_HANDLER(WorkerMsg_StartWorkerContext, OnStartWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_Connect, OnConnect)
  IPC_END_MESSAGE_MAP()
}

void WebSharedWorkerStub::OnStartWorkerContext(
    const GURL& url, const string16& user_agent, const string16& source_code) {
  impl_->startWorkerContext(url, name_, user_agent, source_code);
}

void WebSharedWorkerStub::OnConnect(int sent_message_port_id, int routing_id) {
  WebKit::WebMessagePortChannel* channel =
      new WebMessagePortChannelImpl(routing_id, sent_message_port_id);
  impl_->connect(channel, NULL);
}

void WebSharedWorkerStub::OnTerminateWorkerContext() {
  impl_->terminateWorkerContext();

  // Call the client to make sure context exits.
  EnsureWorkerContextTerminates();
}
