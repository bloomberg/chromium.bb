// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    EmbeddedWorkerRegistry* worker_registry,
    int64 version_id)
    : version_id_(version_id),
      is_shutdown_(false),
      registration_(registration) {
  if (worker_registry)
    embedded_worker_ = worker_registry->CreateWorker();
}

ServiceWorkerVersion::~ServiceWorkerVersion() { DCHECK(is_shutdown_); }

void ServiceWorkerVersion::Shutdown() {
  is_shutdown_ = true;
  registration_ = NULL;
  embedded_worker_.reset();
}

void ServiceWorkerVersion::StartWorker() {
  DCHECK(!is_shutdown_);
  DCHECK(registration_);
  embedded_worker_->Start(version_id_, registration_->script_url());
}

void ServiceWorkerVersion::StopWorker() {
  DCHECK(!is_shutdown_);
  embedded_worker_->Stop();
}

void ServiceWorkerVersion::OnAssociateProvider(
    ServiceWorkerProviderHost* provider_host) {
  DCHECK(!is_shutdown_);
  embedded_worker_->AddProcessReference(provider_host->process_id());
}

void ServiceWorkerVersion::OnUnassociateProvider(
    ServiceWorkerProviderHost* provider_host) {
  embedded_worker_->ReleaseProcessReference(provider_host->process_id());
}

}  // namespace content
