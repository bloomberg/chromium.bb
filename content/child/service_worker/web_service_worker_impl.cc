// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_impl.h"

#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebString;

namespace content {

WebServiceWorkerImpl::WebServiceWorkerImpl(
    int handle_id,
    ThreadSafeSender* thread_safe_sender)
    : handle_id_(handle_id),
      thread_safe_sender_(thread_safe_sender) {}

WebServiceWorkerImpl::~WebServiceWorkerImpl() {
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_ServiceWorkerObjectDestroyed(handle_id_));
}

void WebServiceWorkerImpl::postMessage(const WebString& message,
                                       WebMessagePortChannelArray* channels) {
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_PostMessage(
      handle_id_,
      message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(channels)));
}

}  // namespace content
