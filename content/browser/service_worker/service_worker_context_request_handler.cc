// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_request_handler.h"

#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_read_from_cache_job.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"

namespace content {

ServiceWorkerContextRequestHandler::ServiceWorkerContextRequestHandler(
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context,
    ResourceType resource_type)
    : ServiceWorkerRequestHandler(context,
                                  provider_host,
                                  blob_storage_context,
                                  resource_type),
      version_(provider_host_->running_hosted_version()) {
  DCHECK(provider_host_->IsHostToRunningServiceWorker());
}

ServiceWorkerContextRequestHandler::~ServiceWorkerContextRequestHandler() {
}

net::URLRequestJob* ServiceWorkerContextRequestHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  if (!provider_host_ || !version_ || !context_)
    return NULL;

  // We currently have no use case for hijacking a redirected request.
  if (request->url_chain().size() > 1)
    return NULL;

  // We only use the script cache for main script loading and
  // importScripts(), even if a cached script is xhr'd, we don't
  // retrieve it from the script cache.
  // TODO(michaeln): Get the desired behavior clarified in the spec,
  // and make tweak the behavior here to match.
  if (resource_type_ != RESOURCE_TYPE_SERVICE_WORKER &&
      resource_type_ != RESOURCE_TYPE_SCRIPT) {
    return NULL;
  }

  if (ShouldAddToScriptCache(request->url())) {
    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(version_->registration_id());
    DCHECK(registration);  // We're registering or updating so must be there.

    int64 response_id = context_->storage()->NewResourceId();
    if (response_id == kInvalidServiceWorkerResponseId)
      return NULL;

    // Bypass the browser cache for initial installs and update
    // checks after 24 hours have passed.
    int extra_load_flags = 0;
    base::TimeDelta time_since_last_check =
        base::Time::Now() - registration->last_update_check();
    if (time_since_last_check > base::TimeDelta::FromHours(24))
      extra_load_flags = net::LOAD_BYPASS_CACHE;

    return new ServiceWorkerWriteToCacheJob(request,
                                            network_delegate,
                                            resource_type_,
                                            context_,
                                            version_,
                                            extra_load_flags,
                                            response_id);
  }

  int64 response_id = kInvalidServiceWorkerResponseId;
  if (ShouldReadFromScriptCache(request->url(), &response_id)) {
    return new ServiceWorkerReadFromCacheJob(
        request, network_delegate, context_, response_id);
  }

  // NULL means use the network.
  return NULL;
}

void ServiceWorkerContextRequestHandler::GetExtraResponseInfo(
    bool* was_fetched_via_service_worker,
    GURL* original_url_via_service_worker) const {
  *was_fetched_via_service_worker = false;
  *original_url_via_service_worker = GURL();
}

bool ServiceWorkerContextRequestHandler::ShouldAddToScriptCache(
    const GURL& url) {
  // We only write imports that occur during the initial eval.
  if (version_->status() != ServiceWorkerVersion::NEW &&
      version_->status() != ServiceWorkerVersion::INSTALLING) {
    return false;
  }
  return version_->script_cache_map()->Lookup(url) ==
            kInvalidServiceWorkerResponseId;
}

bool ServiceWorkerContextRequestHandler::ShouldReadFromScriptCache(
    const GURL& url, int64* response_id_out) {
  // We don't read from the script cache until the version is INSTALLED.
  if (version_->status() == ServiceWorkerVersion::NEW ||
      version_->status() == ServiceWorkerVersion::INSTALLING)
    return false;
  *response_id_out = version_->script_cache_map()->Lookup(url);
  return *response_id_out != kInvalidServiceWorkerResponseId;
}

}  // namespace content
