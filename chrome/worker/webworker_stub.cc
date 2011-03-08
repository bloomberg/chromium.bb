// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworker_stub.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "content/common/child_thread.h"
#include "content/common/file_system/file_system_dispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorker.h"

using WebKit::WebWorker;

WebWorkerStub::WebWorkerStub(const GURL& url, int route_id,
                             const WorkerAppCacheInitInfo& appcache_init_info)
    : WebWorkerStubBase(route_id, appcache_init_info),
      ALLOW_THIS_IN_INITIALIZER_LIST(impl_(WebWorker::create(client()))),
      url_(url) {
}

WebWorkerStub::~WebWorkerStub() {
  impl_->clientDestroyed();
}

void WebWorkerStub::OnChannelError() {
    OnTerminateWorkerContext();
}

const GURL& WebWorkerStub::url() const {
  return url_;
}

bool WebWorkerStub::OnMessageReceived(const IPC::Message& message) {
  if (!impl_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebWorkerStub, message)
    IPC_MESSAGE_FORWARD(WorkerMsg_StartWorkerContext, impl_,
                        WebWorker::startWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_FORWARD(WorkerMsg_WorkerObjectDestroyed, impl_,
                        WebWorker::workerObjectDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebWorkerStub::OnTerminateWorkerContext() {
  impl_->terminateWorkerContext();

  // Call the client to make sure context exits.
  EnsureWorkerContextTerminates();
}

void WebWorkerStub::OnPostMessage(
    const string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  WebKit::WebMessagePortChannelArray channels(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); i++) {
    channels[i] = new WebMessagePortChannelImpl(
        new_routing_ids[i], sent_message_port_ids[i]);
  }

  impl_->postMessageToWorkerContext(message, channels);
}
