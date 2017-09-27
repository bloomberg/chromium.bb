// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProxy.h"

using blink::WebMessagePortChannel;
using blink::WebMessagePortChannelArray;
using blink::WebMessagePortChannelClient;
using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

namespace {

class HandleImpl : public blink::WebServiceWorker::Handle {
 public:
  explicit HandleImpl(const scoped_refptr<WebServiceWorkerImpl>& worker)
      : worker_(worker) {}
  ~HandleImpl() override {}

  blink::WebServiceWorker* ServiceWorker() override { return worker_.get(); }

 private:
  scoped_refptr<WebServiceWorkerImpl> worker_;

  DISALLOW_COPY_AND_ASSIGN(HandleImpl);
};

}  // namespace

WebServiceWorkerImpl::WebServiceWorkerImpl(
    std::unique_ptr<ServiceWorkerHandleReference> handle_ref,
    ThreadSafeSender* thread_safe_sender)
    : handle_ref_(std::move(handle_ref)),
      state_(handle_ref_->state()),
      thread_safe_sender_(thread_safe_sender),
      proxy_(nullptr) {
  DCHECK_NE(kInvalidServiceWorkerHandleId, handle_ref_->handle_id());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorker(handle_ref_->handle_id(), this);
}

void WebServiceWorkerImpl::OnStateChanged(
    blink::mojom::ServiceWorkerState new_state) {
  state_ = new_state;

  // TODO(nhiroki): This is a quick fix for http://crbug.com/507110
  DCHECK(proxy_);
  if (proxy_)
    proxy_->DispatchStateChangeEvent();
}

void WebServiceWorkerImpl::SetProxy(blink::WebServiceWorkerProxy* proxy) {
  proxy_ = proxy;
}

blink::WebServiceWorkerProxy* WebServiceWorkerImpl::Proxy() {
  return proxy_;
}

blink::WebURL WebServiceWorkerImpl::Url() const {
  return handle_ref_->url();
}

blink::mojom::ServiceWorkerState WebServiceWorkerImpl::GetState() const {
  return state_;
}

void WebServiceWorkerImpl::PostMessage(
    blink::WebServiceWorkerProvider* provider,
    const WebString& message,
    const WebSecurityOrigin& source_origin,
    WebMessagePortChannelArray channels) {
  thread_safe_sender_->Send(new ServiceWorkerHostMsg_PostMessageToWorker(
      handle_ref_->handle_id(),
      static_cast<WebServiceWorkerProviderImpl*>(provider)->provider_id(),
      message.Utf16(), url::Origin(source_origin),
      WebMessagePortChannelImpl::ExtractMessagePorts(std::move(channels))));
}

void WebServiceWorkerImpl::Terminate() {
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_TerminateWorker(handle_ref_->handle_id()));
}

// static
std::unique_ptr<blink::WebServiceWorker::Handle>
WebServiceWorkerImpl::CreateHandle(
    const scoped_refptr<WebServiceWorkerImpl>& worker) {
  if (!worker)
    return nullptr;
  return base::MakeUnique<HandleImpl>(worker);
}

WebServiceWorkerImpl::~WebServiceWorkerImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorker(handle_ref_->handle_id());
}

}  // namespace content
