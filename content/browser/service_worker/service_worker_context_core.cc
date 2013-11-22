// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/common/content_switches.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {

namespace {

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("ServiceWorker");

}  // namespace

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : quota_manager_proxy_(quota_manager_proxy) {
  if (!user_data_directory.empty())
    path_ = user_data_directory.Append(kServiceWorkerDirectory);
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
}

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

}  // namespace content
