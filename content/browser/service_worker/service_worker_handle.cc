// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

scoped_ptr<ServiceWorkerHandle> ServiceWorkerHandle::Create(
    base::WeakPtr<ServiceWorkerContextCore> context,
    IPC::Sender* sender,
    int thread_id,
    ServiceWorkerVersion* version) {
  if (!context || !version)
    return scoped_ptr<ServiceWorkerHandle>();
  ServiceWorkerRegistration* registration =
      context->GetLiveRegistration(version->registration_id());
  return make_scoped_ptr(
      new ServiceWorkerHandle(context, sender, thread_id,
                              registration, version));
}

ServiceWorkerHandle::ServiceWorkerHandle(
    base::WeakPtr<ServiceWorkerContextCore> context,
    IPC::Sender* sender,
    int thread_id,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version)
    : context_(context),
      sender_(sender),
      thread_id_(thread_id),
      registration_(registration),
      version_(version) {
}

ServiceWorkerHandle::~ServiceWorkerHandle() {
  // TODO(kinuko): At this point we can discard the registration if
  // all documents/handles that have a reference to the registration is
  // closed or freed up, but could also keep it alive in cache
  // (e.g. in context_) for a while with some timer so that we don't
  // need to re-load the same registration from disk over and over.
}

void ServiceWorkerHandle::OnVersionStateChanged(ServiceWorkerVersion* version) {
  // TODO(kinuko): Implement.
}

}  // namespace content
