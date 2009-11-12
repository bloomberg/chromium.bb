// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/webworker_stub.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "chrome/worker/nativewebworker_impl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorker.h"

using WebKit::WebWorker;

static bool UrlIsNativeWorker(const GURL& url) {
  // If the renderer was not passed the switch to enable native workers,
  // then the URL should be treated as a JavaScript worker.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableNativeWebWorkers)) {
    return false;
  }
  // Based on the suffix, decide whether the url should be considered
  // a NativeWebWorker (for .nexe) or a WebWorker (for anything else).
  const std::string kNativeSuffix(".nexe");
  std::string worker_url = url.path();
  // Compute the start index of the suffix.
  std::string::size_type suffix_index =
      worker_url.length() - kNativeSuffix.length();
  std::string::size_type pos = worker_url.find(kNativeSuffix, suffix_index);
  return (suffix_index == pos);
}

WebWorkerStub::WebWorkerStub(const GURL& url, int route_id)
    : WebWorkerStubBase(route_id) {
  if (UrlIsNativeWorker(url)) {
    // Launch a native worker.
    impl_ = NativeWebWorkerImpl::create(client());
  } else {
    // Launch a JavaScript worker.
    impl_ = WebKit::WebWorker::create(client());
  }
}

WebWorkerStub::~WebWorkerStub() {
  impl_->clientDestroyed();
}

void WebWorkerStub::OnMessageReceived(const IPC::Message& message) {
  if (!impl_)
    return;

  IPC_BEGIN_MESSAGE_MAP(WebWorkerStub, message)
    IPC_MESSAGE_FORWARD(WorkerMsg_StartWorkerContext, impl_,
                        WebWorker::startWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_TerminateWorkerContext,
                        OnTerminateWorkerContext)
    IPC_MESSAGE_HANDLER(WorkerMsg_PostMessage, OnPostMessage)
    IPC_MESSAGE_FORWARD(WorkerMsg_WorkerObjectDestroyed, impl_,
                        WebWorker::workerObjectDestroyed)
  IPC_END_MESSAGE_MAP()
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
