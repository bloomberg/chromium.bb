// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "webkit/browser/quota/quota_manager.h"

using content::BrowserThread;
using content::BrowserContext;

// static
BrowsingDataQuotaHelper* BrowsingDataQuotaHelper::Create(Profile* profile) {
  return new BrowsingDataQuotaHelperImpl(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get(),
      BrowserContext::GetDefaultStoragePartition(profile)->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK_EQ(false, callback.is_null());
  DCHECK(callback_.is_null());
  DCHECK(!is_fetching_);
  callback_ = callback;
  quota_info_.clear();
  is_fetching_ = true;

  FetchQuotaInfo();
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuota(const std::string& host) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&BrowsingDataQuotaHelperImpl::RevokeHostQuota, this, host));
    return;
  }

  quota_manager_->SetPersistentHostQuota(
      host, 0,
      base::Bind(&BrowsingDataQuotaHelperImpl::DidRevokeHostQuota,
                 weak_factory_.GetWeakPtr()));
}

BrowsingDataQuotaHelperImpl::BrowsingDataQuotaHelperImpl(
    base::MessageLoopProxy* ui_thread,
    base::MessageLoopProxy* io_thread,
    quota::QuotaManager* quota_manager)
    : BrowsingDataQuotaHelper(io_thread),
      quota_manager_(quota_manager),
      is_fetching_(false),
      ui_thread_(ui_thread),
      io_thread_(io_thread),
      weak_factory_(this) {
  DCHECK(quota_manager);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() {}

void BrowsingDataQuotaHelperImpl::FetchQuotaInfo() {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&BrowsingDataQuotaHelperImpl::FetchQuotaInfo, this));
    return;
  }

  quota_manager_->GetOriginsModifiedSince(
      quota::kStorageTypeTemporary,
      base::Time(),
      base::Bind(&BrowsingDataQuotaHelperImpl::GotOrigins,
                 weak_factory_.GetWeakPtr()));
}

void BrowsingDataQuotaHelperImpl::GotOrigins(
    const std::set<GURL>& origins, quota::StorageType type) {
  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end();
       ++itr)
    if (BrowsingDataHelper::HasWebScheme(*itr))
      pending_hosts_.insert(std::make_pair(itr->host(), type));

  DCHECK(type == quota::kStorageTypeTemporary ||
         type == quota::kStorageTypePersistent ||
         type == quota::kStorageTypeSyncable);

  // Calling GetOriginsModifiedSince() for all types by chaining callbacks.
  if (type == quota::kStorageTypeTemporary) {
    quota_manager_->GetOriginsModifiedSince(
        quota::kStorageTypePersistent,
        base::Time(),
        base::Bind(&BrowsingDataQuotaHelperImpl::GotOrigins,
                   weak_factory_.GetWeakPtr()));
  } else if (type == quota::kStorageTypePersistent) {
    quota_manager_->GetOriginsModifiedSince(
        quota::kStorageTypeSyncable,
        base::Time(),
        base::Bind(&BrowsingDataQuotaHelperImpl::GotOrigins,
                   weak_factory_.GetWeakPtr()));
  } else {
    DCHECK(type == quota::kStorageTypeSyncable);
    ProcessPendingHosts();
  }
}

void BrowsingDataQuotaHelperImpl::ProcessPendingHosts() {
  if (pending_hosts_.empty()) {
    OnComplete();
    return;
  }

  PendingHosts::iterator itr = pending_hosts_.begin();
  std::string host = itr->first;
  quota::StorageType type = itr->second;
  pending_hosts_.erase(itr);
  GetHostUsage(host, type);
}

void BrowsingDataQuotaHelperImpl::GetHostUsage(const std::string& host,
                                               quota::StorageType type) {
  DCHECK(quota_manager_.get());
  quota_manager_->GetHostUsage(
      host, type,
      base::Bind(&BrowsingDataQuotaHelperImpl::GotHostUsage,
                 weak_factory_.GetWeakPtr(), host, type));
}

void BrowsingDataQuotaHelperImpl::GotHostUsage(const std::string& host,
                                               quota::StorageType type,
                                               int64 usage) {
  switch (type) {
    case quota::kStorageTypeTemporary:
      quota_info_[host].temporary_usage = usage;
      break;
    case quota::kStorageTypePersistent:
      quota_info_[host].persistent_usage = usage;
      break;
    case quota::kStorageTypeSyncable:
      quota_info_[host].syncable_usage = usage;
      break;
    default:
      NOTREACHED();
  }
  ProcessPendingHosts();
}

void BrowsingDataQuotaHelperImpl::OnComplete() {
  if (!ui_thread_->BelongsToCurrentThread()) {
    ui_thread_->PostTask(
        FROM_HERE,
        base::Bind(&BrowsingDataQuotaHelperImpl::OnComplete, this));
    return;
  }

  is_fetching_ = false;

  QuotaInfoArray result;

  for (std::map<std::string, QuotaInfo>::iterator itr = quota_info_.begin();
       itr != quota_info_.end();
       ++itr) {
    QuotaInfo* info = &itr->second;
    // Skip unused entries
    if (info->temporary_usage <= 0 &&
        info->persistent_usage <= 0 &&
        info->syncable_usage <= 0)
      continue;

    info->host = itr->first;
    result.push_back(*info);
  }

  callback_.Run(result);
  callback_.Reset();
}

void BrowsingDataQuotaHelperImpl::DidRevokeHostQuota(
    quota::QuotaStatusCode status_unused,
    int64 quota_unused) {
}
