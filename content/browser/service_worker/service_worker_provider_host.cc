// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int process_id, int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : process_id_(process_id),
      provider_id_(provider_id),
      context_(context) {
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  AssociateVersion(NULL);
}

void ServiceWorkerProviderHost::AddScriptClient(int thread_id) {
  DCHECK(!ContainsKey(script_client_thread_ids_, thread_id));
  script_client_thread_ids_.insert(thread_id);
}

void ServiceWorkerProviderHost::RemoveScriptClient(int thread_id) {
  DCHECK(ContainsKey(script_client_thread_ids_, thread_id));
  script_client_thread_ids_.erase(thread_id);
}

void ServiceWorkerProviderHost::AssociateVersion(
    ServiceWorkerVersion* version) {
  if (associated_version())
    associated_version_->RemoveProcessToWorker(process_id_);
  associated_version_ = version;
  if (version)
    version->AddProcessToWorker(process_id_);
}

bool ServiceWorkerProviderHost::SetHostedVersionId(int64 version_id) {
  if (!context_)
    return true;  // System is shutting down.
  if (associated_version_)
    return false;  // Unexpected bad message.

  ServiceWorkerVersion* live_version = context_->GetLiveVersion(version_id);
  if (!live_version)
    return true;  // Was deleted before it got started.

  ServiceWorkerVersionInfo info = live_version->GetInfo();
  if (info.running_status != ServiceWorkerVersion::STARTING ||
      info.process_id != process_id_) {
    // If we aren't trying to start this version in our process
    // something is amiss.
    return false;
  }

  hosted_version_ = live_version;
  return true;
}

bool ServiceWorkerProviderHost::ShouldHandleRequest(
    ResourceType::Type resource_type) const {
  if (ServiceWorkerUtils::IsMainResourceType(resource_type))
    return true;

  if (associated_version())
    return true;

  // TODO(kinuko): Handle ServiceWorker cases.
  // For now we always return false here, so that we don't handle
  // requests for ServiceWorker (either for the main or sub resources).

  return false;
}

}  // namespace content
