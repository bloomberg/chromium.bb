// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/renderer/service_worker/service_worker_dispatcher.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProxy.h"

using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

namespace {

class ServiceWorkerHandleImpl : public blink::WebServiceWorker::Handle {
 public:
  explicit ServiceWorkerHandleImpl(scoped_refptr<WebServiceWorkerImpl> worker)
      : worker_(std::move(worker)) {}
  ~ServiceWorkerHandleImpl() override {}

  blink::WebServiceWorker* ServiceWorker() override { return worker_.get(); }

 private:
  scoped_refptr<WebServiceWorkerImpl> worker_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandleImpl);
};

void OnTerminated(
    std::unique_ptr<WebServiceWorkerImpl::TerminateForTestingCallback>
        callback) {
  callback->OnSuccess();
}

}  // namespace

// static
scoped_refptr<WebServiceWorkerImpl>
WebServiceWorkerImpl::CreateForServiceWorkerGlobalScope(
    blink::mojom::ServiceWorkerObjectInfoPtr info,
    ThreadSafeSender* thread_safe_sender,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  scoped_refptr<WebServiceWorkerImpl> impl =
      new WebServiceWorkerImpl(std::move(info), thread_safe_sender);
  impl->host_for_global_scope_ =
      blink::mojom::ThreadSafeServiceWorkerObjectHostAssociatedPtr::Create(
          std::move(impl->info_->host_ptr_info), io_task_runner);
  return impl;
}

// static
scoped_refptr<WebServiceWorkerImpl>
WebServiceWorkerImpl::CreateForServiceWorkerClient(
    blink::mojom::ServiceWorkerObjectInfoPtr info,
    ThreadSafeSender* thread_safe_sender) {
  scoped_refptr<WebServiceWorkerImpl> impl =
      new WebServiceWorkerImpl(std::move(info), thread_safe_sender);
  impl->host_for_client_.Bind(std::move(impl->info_->host_ptr_info));
  return impl;
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
  return info_->url;
}

blink::mojom::ServiceWorkerState WebServiceWorkerImpl::GetState() const {
  return state_;
}

void WebServiceWorkerImpl::PostMessage(blink::TransferableMessage message,
                                       const WebSecurityOrigin& source_origin) {
  GetObjectHost()->PostMessage(std::move(message), url::Origin(source_origin));
}

void WebServiceWorkerImpl::TerminateForTesting(
    std::unique_ptr<TerminateForTestingCallback> callback) {
  GetObjectHost()->TerminateForTesting(
      base::BindOnce(&OnTerminated, std::move(callback)));
}

// static
std::unique_ptr<blink::WebServiceWorker::Handle>
WebServiceWorkerImpl::CreateHandle(scoped_refptr<WebServiceWorkerImpl> worker) {
  if (!worker)
    return nullptr;
  return std::make_unique<ServiceWorkerHandleImpl>(std::move(worker));
}

WebServiceWorkerImpl::WebServiceWorkerImpl(
    blink::mojom::ServiceWorkerObjectInfoPtr info,
    ThreadSafeSender* thread_safe_sender)
    : info_(std::move(info)),
      state_(info_->state),
      thread_safe_sender_(thread_safe_sender),
      proxy_(nullptr) {
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerHandleId, info_->handle_id);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorker(info_->handle_id, this);
}

WebServiceWorkerImpl::~WebServiceWorkerImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorker(info_->handle_id);
}

blink::mojom::ServiceWorkerObjectHost* WebServiceWorkerImpl::GetObjectHost() {
  if (host_for_client_)
    return host_for_client_.get();
  if (host_for_global_scope_)
    return host_for_global_scope_->get();
  NOTREACHED();
  return nullptr;
}

}  // namespace content
