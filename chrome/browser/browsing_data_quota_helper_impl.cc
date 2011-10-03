// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "webkit/quota/quota_manager.h"

// static
BrowsingDataQuotaHelper* BrowsingDataQuotaHelper::Create(Profile* profile) {
  return new BrowsingDataQuotaHelperImpl(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      profile->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(FetchResultCallback* callback) {
  DCHECK(callback);
  DCHECK(!callback_.get());
  DCHECK(!is_fetching_);
  callback_.reset(callback);
  quota_info_.clear();
  is_fetching_ = true;

  FetchQuotaInfo();
}

void BrowsingDataQuotaHelperImpl::CancelNotification() {
  callback_.reset();
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuota(const std::string& host) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &BrowsingDataQuotaHelperImpl::RevokeHostQuota,
            host));
    return;
  }

  quota_manager_->SetPersistentHostQuota(
      host, 0, callback_factory_.NewCallback(
          &BrowsingDataQuotaHelperImpl::DidRevokeHostQuota));
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
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(quota_manager);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() {}

void BrowsingDataQuotaHelperImpl::FetchQuotaInfo() {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &BrowsingDataQuotaHelperImpl::FetchQuotaInfo));
    return;
  }

  quota_manager_->GetOriginsModifiedSince(
      quota::kStorageTypeTemporary,
      base::Time(),
      callback_factory_.NewCallback(
          &BrowsingDataQuotaHelperImpl::GotOrigins));
}

void BrowsingDataQuotaHelperImpl::GotOrigins(
    const std::set<GURL>& origins, quota::StorageType type) {
  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end();
       ++itr)
    pending_hosts_.insert(std::make_pair(itr->host(), type));

  DCHECK(type == quota::kStorageTypeTemporary ||
         type == quota::kStorageTypePersistent);

  if (type == quota::kStorageTypeTemporary) {
    quota_manager_->GetOriginsModifiedSince(
        quota::kStorageTypePersistent,
        base::Time(),
        callback_factory_.NewCallback(
            &BrowsingDataQuotaHelperImpl::GotOrigins));
  } else {
    // type == quota::kStorageTypePersistent
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
      callback_factory_.NewCallback(
          &BrowsingDataQuotaHelperImpl::GotHostUsage));
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
    default:
      NOTREACHED();
  }
  ProcessPendingHosts();
}

void BrowsingDataQuotaHelperImpl::OnComplete() {
  // Check if CancelNotification was called
  if (!callback_.get())
    return;

  if (!ui_thread_->BelongsToCurrentThread()) {
    ui_thread_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &BrowsingDataQuotaHelperImpl::OnComplete));
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
        info->persistent_usage <= 0)
      continue;

    info->host = itr->first;
    result.push_back(*info);
  }

  callback_->Run(result);
  callback_.reset();
}

void BrowsingDataQuotaHelperImpl::DidRevokeHostQuota(
    quota::QuotaStatusCode status_unused,
    const std::string& host_unused,
    quota::StorageType type_unused,
    int64 quota_unused) {
}
