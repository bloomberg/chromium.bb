// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_provider_context.h"

#include "base/thread_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"

namespace content {

ServiceWorkerProviderContext::ServiceWorkerProviderContext(int provider_id)
    : provider_id_(provider_id),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  if (!ChildThreadImpl::current())
    return;  // May be null in some tests.
  thread_safe_sender_ = ChildThreadImpl::current()->thread_safe_sender();
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
          thread_safe_sender_.get(), main_thread_task_runner_.get());
  DCHECK(dispatcher);
  dispatcher->AddProviderContext(this);
}

ServiceWorkerProviderContext::~ServiceWorkerProviderContext() {
  if (ServiceWorkerDispatcher* dispatcher =
          ServiceWorkerDispatcher::GetThreadSpecificInstance()) {
    // Remove this context from the dispatcher living on the main thread.
    dispatcher->RemoveProviderContext(this);
  }
}

ServiceWorkerHandleReference* ServiceWorkerProviderContext::controller() {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  return controller_.get();
}

bool ServiceWorkerProviderContext::GetRegistrationInfoAndVersionAttributes(
    ServiceWorkerRegistrationObjectInfo* info,
    ServiceWorkerVersionAttributes* attrs) {
  DCHECK(!main_thread_task_runner_->RunsTasksOnCurrentThread());
  base::AutoLock lock(lock_);
  if (!registration_)
    return false;

  *info = registration_->info();
  if (installing_)
    attrs->installing = installing_->info();
  if (waiting_)
    attrs->waiting = waiting_->info();
  if (active_)
    attrs->active = active_->info();
  return true;
}

void ServiceWorkerProviderContext::OnAssociateRegistration(
    const ServiceWorkerRegistrationObjectInfo& info,
    const ServiceWorkerVersionAttributes& attrs) {
  base::AutoLock lock(lock_);
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!registration_);
  DCHECK_NE(kInvalidServiceWorkerRegistrationId, info.registration_id);
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
  base::AutoLock lock(lock_);
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());

  controller_.reset();
  active_.reset();
  waiting_.reset();
  installing_.reset();
  registration_.reset();
}

void ServiceWorkerProviderContext::OnSetControllerServiceWorker(
    const ServiceWorkerObjectInfo& info) {
  DCHECK(main_thread_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(registration_);

  if (info.version_id == kInvalidServiceWorkerVersionId) {
    controller_.reset();
  } else {
    // This context is is the primary owner of this handle, keeps the
    // initial reference until it goes away.
    controller_ =
        ServiceWorkerHandleReference::Adopt(info, thread_safe_sender_.get());
  }
  // TODO(kinuko): We can forward the message to other threads here
  // when we support navigator.serviceWorker in dedicated workers.
}

void ServiceWorkerProviderContext::DestructOnMainThread() const {
  if (!main_thread_task_runner_->RunsTasksOnCurrentThread() &&
      main_thread_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

}  // namespace content
