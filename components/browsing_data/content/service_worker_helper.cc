// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/service_worker_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_usage_info.h"

using content::BrowserThread;
using content::ServiceWorkerContext;
using content::StorageUsageInfo;

namespace browsing_data {
namespace {

void GetAllOriginsInfoForServiceWorkerCallback(
    ServiceWorkerHelper::FetchCallback callback,
    const std::vector<StorageUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const StorageUsageInfo& origin : origins) {
    if (!HasWebScheme(origin.origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }

  content::RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                                 base::BindOnce(std::move(callback), result));
}

}  // namespace

ServiceWorkerHelper::ServiceWorkerHelper(
    ServiceWorkerContext* service_worker_context)
    : service_worker_context_(service_worker_context) {
  DCHECK(service_worker_context_);
}

ServiceWorkerHelper::~ServiceWorkerHelper() {}

void ServiceWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  content::RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &ServiceWorkerHelper::FetchServiceWorkerUsageInfoOnCoreThread, this,
          std::move(callback)));
}

void ServiceWorkerHelper::DeleteServiceWorkers(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&ServiceWorkerHelper::DeleteServiceWorkersOnCoreThread,
                     this, origin));
}

void ServiceWorkerHelper::FetchServiceWorkerUsageInfoOnCoreThread(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!callback.is_null());

  service_worker_context_->GetAllOriginsInfo(base::BindOnce(
      &GetAllOriginsInfoForServiceWorkerCallback, std::move(callback)));
}

void ServiceWorkerHelper::DeleteServiceWorkersOnCoreThread(const GURL& origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  service_worker_context_->DeleteForOrigin(origin, base::DoNothing());
}

CannedServiceWorkerHelper::CannedServiceWorkerHelper(
    content::ServiceWorkerContext* context)
    : ServiceWorkerHelper(context) {}

CannedServiceWorkerHelper::~CannedServiceWorkerHelper() {}

void CannedServiceWorkerHelper::Add(const url::Origin& origin) {
  if (!HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedServiceWorkerHelper::Reset() {
  pending_origins_.clear();
}

bool CannedServiceWorkerHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedServiceWorkerHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedServiceWorkerHelper::GetOrigins() const {
  return pending_origins_;
}

void CannedServiceWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedServiceWorkerHelper::DeleteServiceWorkers(const GURL& origin) {
  pending_origins_.erase(url::Origin::Create(origin));
  ServiceWorkerHelper::DeleteServiceWorkers(origin);
}

}  // namespace browsing_data
