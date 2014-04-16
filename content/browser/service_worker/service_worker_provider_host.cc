// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int process_id, int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
    : process_id_(process_id),
      provider_id_(provider_id),
      context_(context),
      dispatcher_host_(dispatcher_host) {
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  if (active_version_)
    active_version_->RemoveControllee(this);
  if (pending_version_)
    pending_version_->RemovePendingControllee(this);
}

void ServiceWorkerProviderHost::AddScriptClient(int thread_id) {
  DCHECK(!ContainsKey(script_client_thread_ids_, thread_id));
  script_client_thread_ids_.insert(thread_id);
}

void ServiceWorkerProviderHost::RemoveScriptClient(int thread_id) {
  DCHECK(ContainsKey(script_client_thread_ids_, thread_id));
  script_client_thread_ids_.erase(thread_id);
}

void ServiceWorkerProviderHost::SetActiveVersion(
    ServiceWorkerVersion* version) {
  if (version == active_version_)
    return;
  scoped_refptr<ServiceWorkerVersion> previous_version = active_version_;
  active_version_ = version;
  if (version)
    version->AddControllee(this);
  if (previous_version)
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  for (std::set<int>::iterator it = script_client_thread_ids_.begin();
       it != script_client_thread_ids_.end();
       ++it) {
    if (version) {
      dispatcher_host_->RegisterServiceWorkerHandle(
          ServiceWorkerHandle::Create(context_, dispatcher_host_,
                                      *it, version));
    }
    // TODO(kinuko): dispatch activechange event to the script clients.
  }
}

void ServiceWorkerProviderHost::SetPendingVersion(
    ServiceWorkerVersion* version) {
  if (version == pending_version_)
    return;
  scoped_refptr<ServiceWorkerVersion> previous_version = pending_version_;
  pending_version_ = version;
  if (version)
    version->AddPendingControllee(this);
  if (previous_version)
    previous_version->RemovePendingControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  for (std::set<int>::iterator it = script_client_thread_ids_.begin();
       it != script_client_thread_ids_.end();
       ++it) {
    if (version) {
      dispatcher_host_->RegisterServiceWorkerHandle(
          ServiceWorkerHandle::Create(context_, dispatcher_host_,
                                      *it, version));
    }
    // TODO(kinuko): dispatch pendingchange event to the script clients.
  }
}

bool ServiceWorkerProviderHost::SetHostedVersionId(int64 version_id) {
  if (!context_)
    return true;  // System is shutting down.
  if (active_version_)
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

  if (active_version())
    return true;

  // TODO(kinuko): Handle ServiceWorker cases.
  // For now we always return false here, so that we don't handle
  // requests for ServiceWorker (either for the main or sub resources).

  return false;
}

}  // namespace content
