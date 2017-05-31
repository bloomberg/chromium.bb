// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_test_utils.h"

#include <utility>

#include "content/browser/service_worker/service_worker_provider_host.h"

namespace content {

ServiceWorkerRemoteProviderEndpoint::ServiceWorkerRemoteProviderEndpoint() {}
ServiceWorkerRemoteProviderEndpoint::ServiceWorkerRemoteProviderEndpoint(
    ServiceWorkerRemoteProviderEndpoint&& other)
    : host_ptr_(std::move(other.host_ptr_)),
      client_request_(std::move(other.client_request_)) {}

ServiceWorkerRemoteProviderEndpoint::~ServiceWorkerRemoteProviderEndpoint() {}

void ServiceWorkerRemoteProviderEndpoint::BindWithProviderHostInfo(
    content::ServiceWorkerProviderHostInfo* info) {
  mojom::ServiceWorkerProviderAssociatedPtr client_ptr;
  client_request_ = mojo::MakeIsolatedRequest(&client_ptr);
  info->client_ptr_info = client_ptr.PassInterface();
  info->host_request = mojo::MakeIsolatedRequest(&host_ptr_);
}

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostForWindow(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint) {
  ServiceWorkerProviderHostInfo info(provider_id, 1 /* route_id */,
                                     SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                     is_parent_frame_secure);
  output_endpoint->BindWithProviderHostInfo(&info);
  return ServiceWorkerProviderHost::Create(process_id, std::move(info),
                                           std::move(context), nullptr);
}

std::unique_ptr<ServiceWorkerProviderHost>
CreateProviderHostForServiceWorkerContext(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint) {
  ServiceWorkerProviderHostInfo info(provider_id, MSG_ROUTING_NONE,
                                     SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
                                     is_parent_frame_secure);
  output_endpoint->BindWithProviderHostInfo(&info);
  return ServiceWorkerProviderHost::Create(process_id, std::move(info),
                                           std::move(context), nullptr);
}

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostWithDispatcherHost(
    int process_id,
    int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    int route_id,
    ServiceWorkerDispatcherHost* dispatcher_host,
    ServiceWorkerRemoteProviderEndpoint* output_endpoint) {
  ServiceWorkerProviderHostInfo info(provider_id, route_id,
                                     SERVICE_WORKER_PROVIDER_FOR_WINDOW, true);
  output_endpoint->BindWithProviderHostInfo(&info);
  return ServiceWorkerProviderHost::Create(process_id, std::move(info),
                                           std::move(context), dispatcher_host);
}

}  // namespace content
