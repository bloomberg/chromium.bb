// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager.h"

using content::BrowserThread;
using content::BrowserContext;

// static
BrowsingDataQuotaHelper* BrowsingDataQuotaHelper::Create(Profile* profile) {
  return new BrowsingDataQuotaHelperImpl(
      BrowserContext::GetDefaultStoragePartition(profile)->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread, this,
                 callback));
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuota(const std::string& host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataQuotaHelperImpl::RevokeHostQuotaOnIOThread, this,
                 host));
}

BrowsingDataQuotaHelperImpl::BrowsingDataQuotaHelperImpl(
    storage::QuotaManager* quota_manager)
    : BrowsingDataQuotaHelper(),
      quota_manager_(quota_manager),
      weak_factory_(this) {
  DCHECK(quota_manager);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() {}

void BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const storage::StorageType types[] = {storage::kStorageTypeTemporary,
                                        storage::kStorageTypePersistent,
                                        storage::kStorageTypeSyncable};

  // Query hosts for each storage types. When complete, process the collected
  // hosts.
  PendingHosts* pending_hosts = new PendingHosts();
  base::Closure completion = base::BarrierClosure(
      arraysize(types),
      base::Bind(&BrowsingDataQuotaHelperImpl::OnGetOriginsComplete,
                 weak_factory_.GetWeakPtr(), callback,
                 base::Owned(pending_hosts)));

  for (const storage::StorageType& type : types) {
    quota_manager_->GetOriginsModifiedSince(
        type, base::Time(),
        base::Bind(&BrowsingDataQuotaHelperImpl::GotOrigins,
                   weak_factory_.GetWeakPtr(), pending_hosts, completion));
  }
}

void BrowsingDataQuotaHelperImpl::GotOrigins(PendingHosts* pending_hosts,
                                             const base::Closure& completion,
                                             const std::set<GURL>& origins,
                                             storage::StorageType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const GURL& url : origins) {
    if (!BrowsingDataHelper::HasWebScheme(url))
      continue;  // Non-websafe state is not considered browsing data.
    pending_hosts->insert(std::make_pair(url.host(), type));
  }
  completion.Run();
}

void BrowsingDataQuotaHelperImpl::OnGetOriginsComplete(
    const FetchResultCallback& callback,
    PendingHosts* pending_hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Query usage for each (host, type). When complete, process the results.
  QuotaInfoMap* quota_info = new QuotaInfoMap();
  base::Closure completion = base::BarrierClosure(
      pending_hosts->size(),
      base::Bind(&BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete,
                 weak_factory_.GetWeakPtr(), callback,
                 base::Owned(quota_info)));

  for (const auto& itr : *pending_hosts) {
    const std::string& host = itr.first;
    storage::StorageType type = itr.second;
    quota_manager_->GetHostUsage(
        host, type, base::Bind(&BrowsingDataQuotaHelperImpl::GotHostUsage,
                               weak_factory_.GetWeakPtr(), quota_info,
                               completion, host, type));
  }
}

void BrowsingDataQuotaHelperImpl::GotHostUsage(QuotaInfoMap* quota_info,
                                               const base::Closure& completion,
                                               const std::string& host,
                                               storage::StorageType type,
                                               int64 usage) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (type) {
    case storage::kStorageTypeTemporary:
      (*quota_info)[host].temporary_usage = usage;
      break;
    case storage::kStorageTypePersistent:
      (*quota_info)[host].persistent_usage = usage;
      break;
    case storage::kStorageTypeSyncable:
      (*quota_info)[host].syncable_usage = usage;
      break;
    default:
      NOTREACHED();
  }
  completion.Run();
}

void BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete(
    const FetchResultCallback& callback,
    QuotaInfoMap* quota_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  QuotaInfoArray result;
  for (auto& pair : *quota_info) {
    QuotaInfo& info = pair.second;
    // Skip unused entries
    if (info.temporary_usage <= 0 && info.persistent_usage <= 0 &&
        info.syncable_usage <= 0)
      continue;

    info.host = pair.first;
    result.push_back(info);
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, result));
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuotaOnIOThread(
    const std::string& host) {
  quota_manager_->SetPersistentHostQuota(
      host, 0, base::Bind(&BrowsingDataQuotaHelperImpl::DidRevokeHostQuota,
                          weak_factory_.GetWeakPtr()));
}

void BrowsingDataQuotaHelperImpl::DidRevokeHostQuota(
    storage::QuotaStatusCode /*status*/,
    int64 /*quota*/) {}
