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
  if (handle_id_ == kInvalidServiceWorkerHandleId)
    return;
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_ServiceWorkerObjectDestroyed(handle_id_));
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorker(handle_id_);
}

void WebServiceWorkerImpl::OnStateChanged(
    blink::WebServiceWorkerState new_state) {
  DCHECK(proxy_);
  if (proxy_->isReady())
    ChangeState(new_state);
  else
    queued_states_.push_back(new_state);
}

void WebServiceWorkerImpl::setProxy(blink::WebServiceWorkerProxy* proxy) {
  proxy_ = proxy;
}

void WebServiceWorkerImpl::proxyReadyChanged() {
  if (!proxy_->isReady())
    return;
  for (std::vector<blink::WebServiceWorkerState>::iterator it =
           queued_states_.begin();
       it != queued_states_.end();
       ++it) {
    ChangeState(*it);
  }
  queued_states_.clear();
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

void WebServiceWorkerImpl::ChangeState(blink::WebServiceWorkerState new_state) {
  DCHECK(proxy_);
  DCHECK(proxy_->isReady());
  state_ = new_state;
  proxy_->dispatchStateChangeEvent();
}

}  // namespace content
