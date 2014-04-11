// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_impl.h"

#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProxy.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebString;

namespace content {

WebServiceWorkerImpl::WebServiceWorkerImpl(
    const ServiceWorkerObjectInfo& info,
    ThreadSafeSender* thread_safe_sender)
    : handle_id_(info.handle_id),
      scope_(info.scope),
      url_(info.url),
      state_(info.state),
      thread_safe_sender_(thread_safe_sender),
      proxy_(NULL) {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorker(handle_id_, this);
}

WebServiceWorkerImpl::~WebServiceWorkerImpl() {
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_ServiceWorkerObjectDestroyed(handle_id_));
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorker(handle_id_);
}

void WebServiceWorkerImpl::SetState(blink::WebServiceWorkerState new_state) {
  state_ = new_state;
  if (!proxy_)
    return;
  proxy_->dispatchStateChangeEvent();
}

void WebServiceWorkerImpl::setProxy(blink::WebServiceWorkerProxy* proxy) {
  proxy_ = proxy;
}

blink::WebURL WebServiceWorkerImpl::scope() const {
  return scope_;
}

blink::WebURL WebServiceWorkerImpl::url() const {
  return url_;
}

blink::WebServiceWorkerState WebServiceWorkerImpl::state() const {
  return state_;
}

void WebServiceWorkerImpl::postMessage(const WebString& message,
                                       WebMessagePortChannelArray* channels) {
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_PostMessage(
      handle_id_,
      message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(channels)));
}

}  // namespace content
