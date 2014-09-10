// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_provider_context.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

ServiceWorkerProviderContext::ServiceWorkerProviderContext(int provider_id)
    : provider_id_(provider_id),
      main_thread_loop_proxy_(base::MessageLoopProxy::current()) {
  if (!ChildThread::current())
    return;  // May be null in some tests.
  thread_safe_sender_ = ChildThread::current()->thread_safe_sender();
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          thread_safe_sender_.get());
  DCHECK(dispatcher);
  dispatcher->AddProviderContext(this);
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (ServiceWorkerDispatcher* dispatcher =
          ServiceWorkerDispatcher::GetThreadSpecificInstance()) {
    dispatcher->RemoveProviderContext(this);
  }
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::installing() {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return installing_.get();
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::waiting() {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return waiting_.get();
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::active() {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return active_.get();
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::controller() {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return controller_.get();
}

ServiceWorkerRegistrationHandleReference*
ServiceWorkerProviderContext::registration() {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return registration_.get();
}

ServiceWorkerVersionAttributes
ServiceWorkerProviderContext::GetVersionAttributes() {
  ServiceWorkerVersionAttributes attrs;
  if (installing())
    attrs.installing = installing()->info();
  if (waiting())
    attrs.waiting = waiting()->info();
  if (active())
    attrs.active = active()->info();
  return attrs;
}

void ServiceWorkerProviderContext::OnAssociateRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  DCHECK(!registration_);
  DCHECK_NE(kInvalidServiceWorkerRegistrationHandleId, info.handle_id);
  registration_ = ServiceWorkerRegistrationHandleReference::Adopt(
      info, thread_safe_sender_.get());
  installing_ = ServiceWorkerHandleReference::Adopt(
      attrs.installing, thread_safe_sender_.get());
  waiting_ = ServiceWorkerHandleReference::Adopt(
      attrs.waiting, thread_safe_sender_.get());
  active_ = ServiceWorkerHandleReference::Adopt(
      attrs.active, thread_safe_sender_.get());
}

void ServiceWorkerProviderContext::OnDisassociateRegistration() {
  controller_.reset();
  active_.reset();
  waiting_.reset();
  installing_.reset();
  registration_.reset();
}

void ServiceWorkerProviderContext::OnServiceWorkerStateChanged(
    int handle_id,
    blink::WebServiceWorkerState state) {
  ServiceWorkerHandleReference* which = NULL;
  if (handle_id == controller_handle_id())
    which = controller_.get();
  else if (handle_id == active_handle_id())
    which = active_.get();
  else if (handle_id == waiting_handle_id())
    which = waiting_.get();
  else if (handle_id == installing_handle_id())
    which = installing_.get();

  // We should only get messages for ServiceWorkers associated with
  // this provider.
  DCHECK(which);

  which->set_state(state);

  // TODO(kinuko): We can forward the message to other threads here
  // when we support navigator.serviceWorker in dedicated workers.
}

void ServiceWorkerProviderContext::OnSetInstallingServiceWorker(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  DCHECK(IsAssociatedWithRegistration(registration_handle_id));
  installing_ =
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
}

void ServiceWorkerProviderContext::OnSetWaitingServiceWorker(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  DCHECK(IsAssociatedWithRegistration(registration_handle_id));
  waiting_ =
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
}

void ServiceWorkerProviderContext::OnSetActiveServiceWorker(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  DCHECK(IsAssociatedWithRegistration(registration_handle_id));
  active_ =
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
}

void ServiceWorkerProviderContext::OnSetControllerServiceWorker(
    int registration_handle_id,
    const ServiceWorkerObjectInfo& info) {
  DCHECK(IsAssociatedWithRegistration(registration_handle_id));

  // This context is is the primary owner of this handle, keeps the
  // initial reference until it goes away.
  controller_ =
      ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());

  // TODO(kinuko): We can forward the message to other threads here
  // when we support navigator.serviceWorker in dedicated workers.
}

int ServiceWorkerProviderContext::installing_handle_id() const {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return installing_ ? installing_->info().handle_id
                     : kInvalidServiceWorkerHandleId;
}

int ServiceWorkerProviderContext::waiting_handle_id() const {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return waiting_ ? waiting_->info().handle_id
                  : kInvalidServiceWorkerHandleId;
}

int ServiceWorkerProviderContext::active_handle_id() const {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return active_ ? active_->info().handle_id
                 : kInvalidServiceWorkerHandleId;
}

int ServiceWorkerProviderContext::controller_handle_id() const {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return controller_ ? controller_->info().handle_id
                     : kInvalidServiceWorkerHandleId;
}

int ServiceWorkerProviderContext::registration_handle_id() const {
  DCHECK(main_thread_loop_proxy_->RunsTasksOnCurrentThread());
  return registration_ ? registration_->info().handle_id
                       : kInvalidServiceWorkerRegistrationHandleId;
}

bool ServiceWorkerProviderContext::IsAssociatedWithRegistration(
    int registration_handle_id) const {
  if (!registration_)
    return false;
  if (registration_handle_id == kInvalidServiceWorkerRegistrationHandleId)
    return false;
  return registration_->info().handle_id == registration_handle_id;
}

}  // namespace content
