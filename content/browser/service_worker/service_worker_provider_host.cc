// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int process_id, int provider_id)
    : process_id_(process_id), provider_id_(provider_id) {
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
}

void ServiceWorkerProviderHost::AddScriptClient(int thread_id) {
  DCHECK(!ContainsKey(script_client_thread_ids_, thread_id));
  script_client_thread_ids_.insert(thread_id);
}

void ServiceWorkerProviderHost::RemoveScriptClient(int thread_id) {
  DCHECK(ContainsKey(script_client_thread_ids_, thread_id));
  script_client_thread_ids_.erase(thread_id);
}

bool ServiceWorkerProviderHost::ShouldHandleRequest(
    ResourceType::Type resource_type) const {
  if (ServiceWorkerUtils::IsMainResourceType(resource_type))
    return true;

  if (associated_version())
    return true;

  return false;
}

}  // namespace content
