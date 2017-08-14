// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_context_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

namespace content {

CacheStorageContextImpl::CacheStorageContextImpl(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CacheStorageContextImpl::~CacheStorageContextImpl() {
}

void CacheStorageContextImpl::Init(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_incognito_ = user_data_directory.empty();
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // This thread-hopping antipattern is needed here for some unit tests, where
  // browser threads are collapsed the quota manager is initialized before the
  // posted task can register the quota client.
  // TODO: Fix the tests to let the quota manager initialize normally.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    CreateCacheStorageManager(user_data_directory, cache_task_runner,
                              std::move(quota_manager_proxy));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::CreateCacheStorageManager, this,
                     user_data_directory, cache_task_runner,
                     std::move(quota_manager_proxy)));
}

void CacheStorageContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CacheStorageContextImpl::ShutdownOnIO, this));
}

CacheStorageManager* CacheStorageContextImpl::cache_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return cache_manager_.get();
}

void CacheStorageContextImpl::SetBlobParametersForCache(
    net::URLRequestContextGetter* request_context_getter,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (cache_manager_ && request_context_getter && blob_storage_context) {
    cache_manager_->SetBlobParametersForCache(
        request_context_getter, blob_storage_context->context()->AsWeakPtr());
  }
}

void CacheStorageContextImpl::GetAllOriginsInfo(
    const CacheStorageContext::GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(callback, std::vector<CacheStorageUsageInfo>()));
    return;
  }

  cache_manager_->GetAllOriginsUsage(callback);
}

void CacheStorageContextImpl::DeleteForOrigin(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (cache_manager_)
    cache_manager_->DeleteOriginData(origin);
}

void CacheStorageContextImpl::AddObserver(
    CacheStorageContextImpl::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (cache_manager_)
    cache_manager_->AddObserver(observer);
}

void CacheStorageContextImpl::RemoveObserver(
    CacheStorageContextImpl::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (cache_manager_)
    cache_manager_->RemoveObserver(observer);
}

void CacheStorageContextImpl::CreateCacheStorageManager(
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!cache_manager_);
  cache_manager_ = CacheStorageManager::Create(
      user_data_directory, cache_task_runner, std::move(quota_manager_proxy));
}

void CacheStorageContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  cache_manager_.reset();
}

}  // namespace content
