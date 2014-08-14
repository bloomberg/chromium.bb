// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_registration_impl.h"

#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRegistrationProxy.h"

namespace content {

WebServiceWorkerRegistrationImpl::WebServiceWorkerRegistrationImpl(
    scoped_ptr<ServiceWorkerRegistrationHandleReference> handle_ref)
    : handle_ref_(handle_ref.Pass()),
      proxy_(NULL) {
  DCHECK(handle_ref_);
  DCHECK_NE(kInvalidServiceWorkerRegistrationHandleId,
            handle_ref_->handle_id());
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->AddServiceWorkerRegistration(handle_ref_->handle_id(), this);
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorkerRegistration(handle_ref_->handle_id());
}

void WebServiceWorkerRegistrationImpl::OnUpdateFound() {
  DCHECK(proxy_);
  proxy_->dispatchUpdateFoundEvent();
}

void WebServiceWorkerRegistrationImpl::setProxy(
    blink::WebServiceWorkerRegistrationProxy* proxy) {
  proxy_ = proxy;
}

blink::WebServiceWorkerRegistrationProxy*
WebServiceWorkerRegistrationImpl::proxy() {
  return proxy_;
}

void WebServiceWorkerRegistrationImpl::setInstalling(
    blink::WebServiceWorker* service_worker) {
  DCHECK(proxy_);
  proxy_->setInstalling(service_worker);
}

void WebServiceWorkerRegistrationImpl::setWaiting(
    blink::WebServiceWorker* service_worker) {
  DCHECK(proxy_);
  proxy_->setWaiting(service_worker);
}

void WebServiceWorkerRegistrationImpl::setActive(
    blink::WebServiceWorker* service_worker) {
  DCHECK(proxy_);
  proxy_->setActive(service_worker);
}

blink::WebURL WebServiceWorkerRegistrationImpl::scope() const {
  return handle_ref_->scope();
}

}  // namespace content
