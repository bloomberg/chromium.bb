// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_test_utils.h"

#include <utility>

#include "content/browser/service_worker/service_worker_provider_host.h"

namespace content {

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostForWindow(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context) {
  ServiceWorkerProviderHostInfo info(provider_id, MSG_ROUTING_NONE,
                                     SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                     is_parent_frame_secure);
  return ServiceWorkerProviderHost::Create(process_id, std::move(info),
                                           std::move(context), nullptr);
}

std::unique_ptr<ServiceWorkerProviderHost>
CreateProviderHostForServiceWorkerContext(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context) {
  ServiceWorkerProviderHostInfo info(provider_id, MSG_ROUTING_NONE,
                                     SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
                                     is_parent_frame_secure);
  return ServiceWorkerProviderHost::Create(process_id, std::move(info),
                                           std::move(context), nullptr);
}

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostWithDispatcherHost(
    int process_id,
    int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    int route_id,
    ServiceWorkerDispatcherHost* dispatcher_host) {
  ServiceWorkerProviderHostInfo info(provider_id, route_id,
                                     SERVICE_WORKER_PROVIDER_FOR_WINDOW, true);
  return ServiceWorkerProviderHost::Create(process_id, std::move(info),
                                           std::move(context), dispatcher_host);
}

}  // namespace content
