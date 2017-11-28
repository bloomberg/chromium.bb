// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_shared_worker_helper.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/shared_worker_service.h"

BrowsingDataSharedWorkerHelper::SharedWorkerInfo::SharedWorkerInfo(
    const GURL& worker,
    const std::string& name)
    : worker(worker), name(name) {}

BrowsingDataSharedWorkerHelper::SharedWorkerInfo::SharedWorkerInfo(
    const SharedWorkerInfo& other) = default;

BrowsingDataSharedWorkerHelper::SharedWorkerInfo::~SharedWorkerInfo() = default;

bool BrowsingDataSharedWorkerHelper::SharedWorkerInfo::operator<(
    const SharedWorkerInfo& other) const {
  return std::tie(worker, name) < std::tie(other.worker, other.name);
}

BrowsingDataSharedWorkerHelper::BrowsingDataSharedWorkerHelper(
    content::StoragePartition* storage_partition,
    content::ResourceContext* resource_context)
    : storage_partition_(storage_partition),
      resource_context_(resource_context) {}

BrowsingDataSharedWorkerHelper::~BrowsingDataSharedWorkerHelper() = default;

void BrowsingDataSharedWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  // We always return an empty list, as there are no "persistent" shared
  // workers.
  std::list<SharedWorkerInfo> result;
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::BindOnce(std::move(callback), result));
}
void BrowsingDataSharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::SharedWorkerService::GetInstance()->TerminateWorker(
      worker, name, storage_partition_, resource_context_);
}

CannedBrowsingDataSharedWorkerHelper::CannedBrowsingDataSharedWorkerHelper(
    content::StoragePartition* storage_partition,
    content::ResourceContext* resource_context)
    : BrowsingDataSharedWorkerHelper(storage_partition, resource_context) {}

CannedBrowsingDataSharedWorkerHelper::~CannedBrowsingDataSharedWorkerHelper() =
    default;

void CannedBrowsingDataSharedWorkerHelper::AddSharedWorker(
    const GURL& worker,
    const std::string& name) {
  if (!BrowsingDataHelper::HasWebScheme(worker))
    return;  // Non-websafe state is not considered browsing data.

  pending_shared_worker_info_.insert(SharedWorkerInfo(worker, name));
}

void CannedBrowsingDataSharedWorkerHelper::Reset() {
  pending_shared_worker_info_.clear();
}

bool CannedBrowsingDataSharedWorkerHelper::empty() const {
  return pending_shared_worker_info_.empty();
}

size_t CannedBrowsingDataSharedWorkerHelper::GetSharedWorkerCount() const {
  return pending_shared_worker_info_.size();
}

const std::set<CannedBrowsingDataSharedWorkerHelper::SharedWorkerInfo>&
CannedBrowsingDataSharedWorkerHelper::GetSharedWorkerInfo() const {
  return pending_shared_worker_info_;
}

void CannedBrowsingDataSharedWorkerHelper::StartFetching(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<SharedWorkerInfo> result;
  for (auto& it : pending_shared_worker_info_)
    result.push_back(it);
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataSharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name) {
  for (auto it = pending_shared_worker_info_.begin();
       it != pending_shared_worker_info_.end();) {
    if (it->worker == worker && it->name == name) {
      BrowsingDataSharedWorkerHelper::DeleteSharedWorker(it->worker, it->name);
      it = pending_shared_worker_info_.erase(it);
    } else {
      ++it;
    }
  }
}
