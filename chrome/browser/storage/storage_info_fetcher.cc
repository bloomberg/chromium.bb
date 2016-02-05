// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_info_fetcher.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserContext;
using content::BrowserThread;

StorageInfoFetcher::StorageInfoFetcher(storage::QuotaManager* quota_manager)
    : quota_manager_(quota_manager) {
}

StorageInfoFetcher::~StorageInfoFetcher() {
}

void StorageInfoFetcher::FetchStorageInfo() {
  // QuotaManager must be called on IO thread, but the callback must then be
  // called on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&StorageInfoFetcher::GetUsageInfo, this,
          base::Bind(&StorageInfoFetcher::OnGetUsageInfoInternal, this)));
}

void StorageInfoFetcher::ClearStorage(
    const std::string& host, storage::StorageType type) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&storage::QuotaManager::DeleteHostData,
                 quota_manager_,
                 host,
                 type,
                 storage::QuotaClient::kAllClientsMask,
                 base::Bind(&StorageInfoFetcher::OnUsageCleared, this)));
}

void StorageInfoFetcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void StorageInfoFetcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void StorageInfoFetcher::GetUsageInfo(
    const storage::GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  quota_manager_->GetUsageInfo(callback);
}

void StorageInfoFetcher::OnGetUsageInfoInternal(
    const storage::UsageInfoEntries& entries) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  entries_.insert(entries_.begin(), entries.begin(), entries.end());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StorageInfoFetcher::InvokeCallback, this));
}

void StorageInfoFetcher::OnUsageCleared(storage::QuotaStatusCode code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  FOR_EACH_OBSERVER(Observer, observers_, OnUsageInfoCleared(code));
}

void StorageInfoFetcher::InvokeCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FOR_EACH_OBSERVER(Observer, observers_, OnGetUsageInfo(entries_));
}
