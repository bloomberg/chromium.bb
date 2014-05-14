// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_script_cache_map.h"

#include "base/logging.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerScriptCacheMap::ServiceWorkerScriptCacheMap(
    ServiceWorkerVersion* owner)
    : owner_(owner),
      is_eval_complete_(false),
      resources_started_(0),
      resources_finished_(0),
      has_error_(false) {
}

ServiceWorkerScriptCacheMap::~ServiceWorkerScriptCacheMap() {
}

int64 ServiceWorkerScriptCacheMap::Lookup(const GURL& url) {
  ResourceIDMap::const_iterator found = resource_ids_.find(url);
  if (found == resource_ids_.end())
    return kInvalidServiceWorkerResponseId;
  return found->second;
}

void ServiceWorkerScriptCacheMap::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ServiceWorkerScriptCacheMap::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceWorkerScriptCacheMap::NotifyStartedCaching(
    const GURL& url, int64 resource_id) {
  DCHECK_EQ(kInvalidServiceWorkerResponseId, Lookup(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING);
  DCHECK(!is_eval_complete_);
  resource_ids_[url] = resource_id;
  ++resources_started_;
}

void ServiceWorkerScriptCacheMap::NotifyFinishedCaching(
    const GURL& url, bool success) {
  DCHECK_NE(kInvalidServiceWorkerResponseId, Lookup(url));
  DCHECK(owner_->status() == ServiceWorkerVersion::NEW ||
         owner_->status() == ServiceWorkerVersion::INSTALLING);
  ++resources_finished_;
  if (!success)
    has_error_ = true;
  if (url == owner_->script_url()) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnMainScriptCached(owner_, success));
  }
  if (is_eval_complete_ && resources_finished_ == resources_started_) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnAllScriptsCached(owner_, has_error_));
  }
}

void ServiceWorkerScriptCacheMap::NotifyEvalCompletion() {
  DCHECK(!is_eval_complete_);
  is_eval_complete_ = true;
  if (resources_finished_ == resources_started_) {
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnAllScriptsCached(owner_, has_error_));
  }
}

}  // namespace content
