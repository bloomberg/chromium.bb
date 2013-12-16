// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& path,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : storage_(new ServiceWorkerStorage(path, quota_manager_proxy)),
      embedded_worker_registry_(new EmbeddedWorkerRegistry(AsWeakPtr())) {}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHost(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  if (!map)
    return NULL;
  return map->Lookup(provider_id);
}

void ServiceWorkerContextCore::AddProviderHost(
    scoped_ptr<ServiceWorkerProviderHost> host) {
  ServiceWorkerProviderHost* host_ptr = host.release();   // we take ownership
  ProviderMap* map = GetProviderMapForProcess(host_ptr->process_id());
  if (!map) {
    map = new ProviderMap;
    providers_.AddWithID(map, host_ptr->process_id());
  }
  map->AddWithID(host_ptr, host_ptr->provider_id());
}

void ServiceWorkerContextCore::RemoveProviderHost(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  DCHECK(map);
  map->Remove(provider_id);
}

void ServiceWorkerContextCore::RemoveAllProviderHostsForProcess(
    int process_id) {
  if (providers_.Lookup(process_id))
    providers_.Remove(process_id);
}

bool ServiceWorkerContextCore::IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableServiceWorker);
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    const RegistrationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  storage_->Register(pattern,
                     script_url,
                     base::Bind(&ServiceWorkerContextCore::RegistrationComplete,
                                AsWeakPtr(),
                                callback));
}

void ServiceWorkerContextCore::UnregisterServiceWorker(
    const GURL& pattern,
    const UnregistrationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  storage_->Unregister(pattern, callback);
}

void ServiceWorkerContextCore::RegistrationComplete(
    const ServiceWorkerContextCore::RegistrationCallback& callback,
    ServiceWorkerRegistrationStatus status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (status != REGISTRATION_OK) {
    DCHECK(!registration);
    callback.Run(status, -1L);
  }

  callback.Run(status, registration->id());
}

}  // namespace content
