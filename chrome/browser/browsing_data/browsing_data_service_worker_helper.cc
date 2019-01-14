// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_usage_info.h"

using content::BrowserThread;
using content::ServiceWorkerContext;
using content::StorageUsageInfo;

namespace {

void GetAllOriginsInfoForServiceWorkerCallback(
    BrowsingDataServiceWorkerHelper::FetchCallback callback,
    const std::vector<StorageUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const StorageUsageInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

}  // namespace

BrowsingDataServiceWorkerHelper::BrowsingDataServiceWorkerHelper(
    ServiceWorkerContext* service_worker_context)
    : service_worker_context_(service_worker_context) {
  DCHECK(service_worker_context_);
}

BrowsingDataServiceWorkerHelper::~BrowsingDataServiceWorkerHelper() {}

void BrowsingDataServiceWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataServiceWorkerHelper::
                         FetchServiceWorkerUsageInfoOnIOThread,
                     this, std::move(callback)));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnIOThread,
          this, origin));
}

void BrowsingDataServiceWorkerHelper::FetchServiceWorkerUsageInfoOnIOThread(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  service_worker_context_->GetAllOriginsInfo(base::BindOnce(
      &GetAllOriginsInfoForServiceWorkerCallback, std::move(callback)));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnIOThread(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->DeleteForOrigin(origin, base::DoNothing());
}

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    PendingServiceWorkerUsageInfo(const GURL& origin,
                                  const std::vector<GURL>& scopes)
    : origin(origin), scopes(scopes) {
}

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    PendingServiceWorkerUsageInfo(const PendingServiceWorkerUsageInfo& other) =
        default;

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    ~PendingServiceWorkerUsageInfo() {
}

bool CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
operator<(const PendingServiceWorkerUsageInfo& other) const {
  return std::tie(origin, scopes) < std::tie(other.origin, other.scopes);
}

CannedBrowsingDataServiceWorkerHelper::CannedBrowsingDataServiceWorkerHelper(
    content::ServiceWorkerContext* context)
    : BrowsingDataServiceWorkerHelper(context) {
}

CannedBrowsingDataServiceWorkerHelper::
    ~CannedBrowsingDataServiceWorkerHelper() {
}

void CannedBrowsingDataServiceWorkerHelper::AddServiceWorker(
    const GURL& origin, const std::vector<GURL>& scopes) {
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  pending_service_worker_info_.insert(
      PendingServiceWorkerUsageInfo(origin, scopes));
}

void CannedBrowsingDataServiceWorkerHelper::Reset() {
  pending_service_worker_info_.clear();
}

bool CannedBrowsingDataServiceWorkerHelper::empty() const {
  return pending_service_worker_info_.empty();
}

size_t CannedBrowsingDataServiceWorkerHelper::GetServiceWorkerCount() const {
  return pending_service_worker_info_.size();
}

const std::set<
    CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo>&
CannedBrowsingDataServiceWorkerHelper::GetServiceWorkerUsageInfo() const {
  return pending_service_worker_info_;
}

void CannedBrowsingDataServiceWorkerHelper::StartFetching(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const PendingServiceWorkerUsageInfo& pending_info :
       pending_service_worker_info_) {
    result.emplace_back(url::Origin::Create(pending_info.origin), 0,
                        base::Time());
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataServiceWorkerHelper::DeleteServiceWorkers(
    const GURL& origin) {
  for (auto it = pending_service_worker_info_.begin();
       it != pending_service_worker_info_.end();) {
    if (it->origin == origin)
      pending_service_worker_info_.erase(it++);
    else
      ++it;
  }
  BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(origin);
}
