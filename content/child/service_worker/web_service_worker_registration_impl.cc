// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_registration_impl.h"

#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRegistrationProxy.h"

namespace content {

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

WebServiceWorkerRegistrationImpl::~WebServiceWorkerRegistrationImpl() {
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveServiceWorkerRegistration(handle_ref_->handle_id());
  ClearQueuedTasks();
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

}  // namespace content
