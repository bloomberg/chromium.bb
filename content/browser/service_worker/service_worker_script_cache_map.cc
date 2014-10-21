// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_cache_map.h"

#include "base/logging.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerScriptCacheMap::ServiceWorkerScriptCacheMap(
    ServiceWorkerVersion* owner,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : owner_(owner),
      context_(context) {
}

ServiceWorkerScriptCacheMap::~ServiceWorkerScriptCacheMap() {
}

int64 ServiceWorkerScriptCacheMap::LookupResourceId(const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end())
    return kInvalidServiceWorkerResponseId;
  return found->second.resource_id;
}

int64 ServiceWorkerScriptCacheMap::LookupResourceSize(const GURL& url) {
  ResourceMap::const_iterator found = resource_map_.find(url);
  if (found == resource_map_.end())
    return kInvalidServiceWorkerResponseId;
  return found->second.size_bytes;
}

void ServiceWorkerScriptCacheMap::NotifyStartedCaching(
    const GURL& url, int64 resource_id) {
  DCHECK_EQ(kInvalidServiceWorkerResponseId, LookupResourceId(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING);
  resource_map_[url] =
      ServiceWorkerDatabase::ResourceRecord(resource_id, url, -1);
  context_->storage()->StoreUncommittedResponseId(resource_id);
}

void ServiceWorkerScriptCacheMap::NotifyFinishedCaching(
    const GURL& url,
    int64 size_bytes,
    const net::URLRequestStatus& status) {
  DCHECK_NE(kInvalidServiceWorkerResponseId, LookupResourceId(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING);
  if (!status.is_success()) {
    context_->storage()->DoomUncommittedResponse(LookupResourceId(url));
    resource_map_.erase(url);
    if (owner_->script_url() == url)
      main_script_status_ = status;
  } else {
    resource_map_[url].size_bytes = size_bytes;
  }
}

void ServiceWorkerScriptCacheMap::GetResources(
    std::vector<ServiceWorkerDatabase::ResourceRecord>* resources) {
  DCHECK(resources->empty());
  for (ResourceMap::const_iterator it = resource_map_.begin();
       it != resource_map_.end();
       ++it) {
    resources->push_back(it->second);
  }
}

void ServiceWorkerScriptCacheMap::SetResources(
    const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources) {
  DCHECK(resource_map_.empty());
  typedef std::vector<ServiceWorkerDatabase::ResourceRecord> RecordVector;
  for (RecordVector::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    resource_map_[it->url] = *it;
  }
}

}  // namespace content
