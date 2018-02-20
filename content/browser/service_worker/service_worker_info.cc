// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_info.h"

#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

ServiceWorkerVersionInfo::ClientInfo::ClientInfo()
    : ClientInfo(ChildProcessHost::kInvalidUniqueID,
                 MSG_ROUTING_NONE,
                 base::Callback<WebContents*(void)>(),
                 blink::mojom::ServiceWorkerProviderType::kUnknown) {}

ServiceWorkerVersionInfo::ClientInfo::ClientInfo(
    int process_id,
    int route_id,
    const base::Callback<WebContents*(void)>& web_contents_getter,
    blink::mojom::ServiceWorkerProviderType type)
    : process_id(process_id),
      route_id(route_id),
      web_contents_getter(web_contents_getter),
      type(type) {}

ServiceWorkerVersionInfo::ClientInfo::ClientInfo(
    const ServiceWorkerVersionInfo::ClientInfo& other) = default;

ServiceWorkerVersionInfo::ClientInfo::~ClientInfo() {
}

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo()
    : running_status(EmbeddedWorkerStatus::STOPPED),
      status(ServiceWorkerVersion::NEW),
      fetch_handler_existence(
          ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN),
      registration_id(blink::mojom::kInvalidServiceWorkerRegistrationId),
      version_id(blink::mojom::kInvalidServiceWorkerVersionId),
      process_id(ChildProcessHost::kInvalidUniqueID),
      thread_id(kInvalidEmbeddedWorkerThreadId),
      devtools_agent_route_id(MSG_ROUTING_NONE) {}

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo(
    EmbeddedWorkerStatus running_status,
    ServiceWorkerVersion::Status status,
    ServiceWorkerVersion::FetchHandlerExistence fetch_handler_existence,
    const GURL& script_url,
    int64_t registration_id,
    int64_t version_id,
    int process_id,
    int thread_id,
    int devtools_agent_route_id)
    : running_status(running_status),
      status(status),
      fetch_handler_existence(fetch_handler_existence),
      script_url(script_url),
      registration_id(registration_id),
      version_id(version_id),
      process_id(process_id),
      thread_id(thread_id),
      devtools_agent_route_id(devtools_agent_route_id) {}

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo(
    const ServiceWorkerVersionInfo& other) = default;

ServiceWorkerVersionInfo::~ServiceWorkerVersionInfo() {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo()
    : registration_id(blink::mojom::kInvalidServiceWorkerRegistrationId),
      delete_flag(IS_NOT_DELETED),
      stored_version_size_bytes(0),
      navigation_preload_enabled(false),
      navigation_preload_header_length(0) {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const GURL& pattern,
    int64_t registration_id,
    DeleteFlag delete_flag)
    : pattern(pattern),
      registration_id(registration_id),
      delete_flag(delete_flag),
      stored_version_size_bytes(0),
      navigation_preload_enabled(false),
      navigation_preload_header_length(0) {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const GURL& pattern,
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
    int64_t registration_id,
    DeleteFlag delete_flag,
    const ServiceWorkerVersionInfo& active_version,
    const ServiceWorkerVersionInfo& waiting_version,
    const ServiceWorkerVersionInfo& installing_version,
    int64_t stored_version_size_bytes,
    bool navigation_preload_enabled,
    size_t navigation_preload_header_length)
    : pattern(pattern),
      update_via_cache(update_via_cache),
      registration_id(registration_id),
      delete_flag(delete_flag),
      active_version(active_version),
      waiting_version(waiting_version),
      installing_version(installing_version),
      stored_version_size_bytes(stored_version_size_bytes),
      navigation_preload_enabled(navigation_preload_enabled),
      navigation_preload_header_length(navigation_preload_header_length) {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const ServiceWorkerRegistrationInfo& other) = default;

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo() {}

}  // namespace content
