// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_registration_impl.h"

#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistrationProxy.h"

namespace content {

namespace {

class HandleImpl : public blink::WebServiceWorkerRegistration::Handle {
 public:
  explicit HandleImpl(WebServiceWorkerRegistrationImpl* registration)
      : registration_(registration) {}
  ~HandleImpl() override {}

  blink::WebServiceWorkerRegistration* registration() override {
    return registration_.get();
  }

 private:
  scoped_refptr<WebServiceWorkerRegistrationImpl> registration_;

  DISALLOW_COPY_AND_ASSIGN(HandleImpl);
};

}  // namespace

WebServiceWorkerRegistrationImpl::QueuedTask::QueuedTask(
    QueuedTaskType type,
    blink::WebServiceWorker* worker)
    : type(type),
      worker(worker) {
}

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

void WebServiceWorkerRegistrationImpl::SetInstalling(
    blink::WebServiceWorker* service_worker) {
  if (proxy_)
    proxy_->setInstalling(service_worker);
  else
    queued_tasks_.push_back(QueuedTask(INSTALLING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetWaiting(
    blink::WebServiceWorker* service_worker) {
  if (proxy_)
    proxy_->setWaiting(service_worker);
  else
    queued_tasks_.push_back(QueuedTask(WAITING, service_worker));
}

void WebServiceWorkerRegistrationImpl::SetActive(
    blink::WebServiceWorker* service_worker) {
  if (proxy_)
    proxy_->setActive(service_worker);
  else
    queued_tasks_.push_back(QueuedTask(ACTIVE, service_worker));
}

void WebServiceWorkerRegistrationImpl::OnUpdateFound() {
  if (proxy_)
    proxy_->dispatchUpdateFoundEvent();
  else
    queued_tasks_.push_back(QueuedTask(UPDATE_FOUND, NULL));
}

void WebServiceWorkerRegistrationImpl::setProxy(
    blink::WebServiceWorkerRegistrationProxy* proxy) {
  proxy_ = proxy;
  RunQueuedTasks();
}

void WebServiceWorkerRegistrationImpl::RunQueuedTasks() {
  DCHECK(proxy_);
  for (std::vector<QueuedTask>::const_iterator it = queued_tasks_.begin();
       it != queued_tasks_.end(); ++it) {
    if (it->type == INSTALLING)
      proxy_->setInstalling(it->worker);
    else if (it->type == WAITING)
      proxy_->setWaiting(it->worker);
    else if (it->type == ACTIVE)
      proxy_->setActive(it->worker);
    else if (it->type == UPDATE_FOUND)
      proxy_->dispatchUpdateFoundEvent();
  }
  queued_tasks_.clear();
}

void WebServiceWorkerRegistrationImpl::ClearQueuedTasks() {
  for (std::vector<QueuedTask>::const_iterator it = queued_tasks_.begin();
       it != queued_tasks_.end(); ++it) {
    // If the owner of the WebServiceWorker does not exist, delete it.
    if (it->worker && !it->worker->proxy())
      delete it->worker;
  }
  queued_tasks_.clear();
}

blink::WebServiceWorkerRegistrationProxy*
WebServiceWorkerRegistrationImpl::proxy() {
  return proxy_;
}

blink::WebURL WebServiceWorkerRegistrationImpl::scope() const {
  return handle_ref_->scope();
}

void WebServiceWorkerRegistrationImpl::update(
    blink::WebServiceWorkerProvider* provider,
    WebServiceWorkerUpdateCallbacks* callbacks) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->UpdateServiceWorker(provider_impl->provider_id(),
                                  registration_id(), callbacks);
}

void WebServiceWorkerRegistrationImpl::unregister(
    blink::WebServiceWorkerProvider* provider,
    WebServiceWorkerUnregistrationCallbacks* callbacks) {
  WebServiceWorkerProviderImpl* provider_impl =
      static_cast<WebServiceWorkerProviderImpl*>(provider);
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  DCHECK(dispatcher);
  dispatcher->UnregisterServiceWorker(provider_impl->provider_id(),
                                      registration_id(), callbacks);
}

int64 WebServiceWorkerRegistrationImpl::registration_id() const {
  return handle_ref_->registration_id();
}

blink::WebPassOwnPtr<blink::WebServiceWorkerRegistration::Handle>
WebServiceWorkerRegistrationImpl::CreateHandle() {
  return blink::adoptWebPtr(new HandleImpl(this));
}

blink::WebServiceWorkerRegistration::Handle*
WebServiceWorkerRegistrationImpl::CreateLeakyHandle() {
  return new HandleImpl(this);
}

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorkerRegistration(handle_ref_->handle_id());
  ClearQueuedTasks();
}

}  // namespace content
