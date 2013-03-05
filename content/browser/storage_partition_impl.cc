// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include "base/utf_string_conversions.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/indexed_db_context.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context.h"
#include "webkit/database/database_tracker.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/quota/quota_manager.h"

namespace content {

namespace {

void DoNothingStatusCallback(quota::QuotaStatusCode status) {
  // Do nothing.
}

void ClearQuotaManagedOriginsOnIOThread(
    const scoped_refptr<quota::QuotaManager>& quota_manager,
    const std::set<GURL>& origins,
    quota::StorageType quota_storage_type) {
  // The QuotaManager manages all storage other than cookies, LocalStorage,
  // and SessionStorage. This loop wipes out most HTML5 storage for the given
  // origins.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::set<GURL>::const_iterator origin;
  for (std::set<GURL>::const_iterator origin = origins.begin();
       origin != origins.end(); ++origin) {
    quota_manager->DeleteOriginData(*origin, quota_storage_type,
                                    quota::QuotaClient::kAllClientsMask,
                                    base::Bind(&DoNothingStatusCallback));
  }
}

void ClearOriginOnIOThread(
    uint32 storage_mask,
    const GURL& storage_origin,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<quota::QuotaManager>& quota_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (storage_mask & StoragePartition::kCookies) {
    // Handle the cookies.
    net::CookieMonster* cookie_monster =
        request_context->GetURLRequestContext()->cookie_store()->
            GetCookieMonster();
    if (cookie_monster)
      cookie_monster->DeleteAllForHostAsync(
          storage_origin, net::CookieMonster::DeleteCallback());
  }

  // Handle all HTML5 storage other than DOMStorageContext.
  std::set<GURL> origins;
  origins.insert(storage_origin);
  if (storage_mask & StoragePartition::kQuotaManagedTemporaryStorage) {
    ClearQuotaManagedOriginsOnIOThread(quota_manager,
                                       origins,
                                       quota::kStorageTypeTemporary);
  }
  if (storage_mask & StoragePartition::kQuotaManagedPersistentStorage) {
    ClearQuotaManagedOriginsOnIOThread(quota_manager,
                                       origins,
                                       quota::kStorageTypePersistent);
  }
}

void ClearAllDataOnIOThread(
    uint32 storage_mask,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<quota::QuotaManager>& quota_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (storage_mask & StoragePartition::kCookies) {
    // Handle the cookies.
    net::CookieMonster* cookie_monster =
        request_context->GetURLRequestContext()->cookie_store()->
            GetCookieMonster();
    if (cookie_monster)
      cookie_monster->DeleteAllAsync(net::CookieMonster::DeleteCallback());
  }

  // Handle all HTML5 storage other than DOMStorageContext.
  if (storage_mask & StoragePartition::kQuotaManagedTemporaryStorage) {
    quota_manager->GetOriginsModifiedSince(
        quota::kStorageTypeTemporary, base::Time(),
        base::Bind(&ClearQuotaManagedOriginsOnIOThread, quota_manager));
  }
  if (storage_mask & StoragePartition::kQuotaManagedPersistentStorage) {
    quota_manager->GetOriginsModifiedSince(
        quota::kStorageTypePersistent, base::Time(),
        base::Bind(&ClearQuotaManagedOriginsOnIOThread, quota_manager));
  }
}

void OnLocalStorageUsageInfo(
    const scoped_refptr<DOMStorageContextImpl>& dom_storage_context,
    const std::vector<dom_storage::LocalStorageUsageInfo>& infos) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < infos.size(); ++i) {
    dom_storage_context->DeleteLocalStorage(infos[i].origin);
  }
}

void OnSessionStorageUsageInfo(
    const scoped_refptr<DOMStorageContextImpl>& dom_storage_context,
    const std::vector<dom_storage::SessionStorageUsageInfo>& infos) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < infos.size(); ++i) {
    dom_storage_context->DeleteSessionStorage(infos[i]);
  }
}

}  // namespace

StoragePartitionImpl::StoragePartitionImpl(
    const base::FilePath& partition_path,
    quota::QuotaManager* quota_manager,
    ChromeAppCacheService* appcache_service,
    fileapi::FileSystemContext* filesystem_context,
    webkit_database::DatabaseTracker* database_tracker,
    DOMStorageContextImpl* dom_storage_context,
    IndexedDBContextImpl* indexed_db_context)
    : partition_path_(partition_path),
      quota_manager_(quota_manager),
      appcache_service_(appcache_service),
      filesystem_context_(filesystem_context),
      database_tracker_(database_tracker),
      dom_storage_context_(dom_storage_context),
      indexed_db_context_(indexed_db_context) {
}

StoragePartitionImpl::~StoragePartitionImpl() {
  // These message loop checks are just to avoid leaks in unittests.
  if (GetDatabaseTracker() &&
      BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&webkit_database::DatabaseTracker::Shutdown,
                   GetDatabaseTracker()));
  }

  if (GetDOMStorageContext())
    GetDOMStorageContext()->Shutdown();
}

// TODO(ajwong): Break the direct dependency on |context|. We only
// need 3 pieces of info from it.
StoragePartitionImpl* StoragePartitionImpl::Create(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& partition_path) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManger being used. We do them
  // all together here prior to handing out a reference to anything
  // that utilizes the QuotaManager.
  scoped_refptr<quota::QuotaManager> quota_manager =
      new quota::QuotaManager(
          in_memory, partition_path,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
          context->GetSpecialStoragePolicy());

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  scoped_refptr<fileapi::FileSystemContext> filesystem_context =
      CreateFileSystemContext(partition_path, in_memory,
                              BrowserContext::GetMountPoints(context),
                              context->GetSpecialStoragePolicy(),
                              quota_manager->proxy());

  scoped_refptr<webkit_database::DatabaseTracker> database_tracker =
      new webkit_database::DatabaseTracker(
          partition_path, in_memory,
          context->GetSpecialStoragePolicy(), quota_manager->proxy(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

  base::FilePath path = in_memory ? base::FilePath() : partition_path;
  scoped_refptr<DOMStorageContextImpl> dom_storage_context =
      new DOMStorageContextImpl(path, context->GetSpecialStoragePolicy());

  scoped_refptr<IndexedDBContextImpl> indexed_db_context =
      new IndexedDBContextImpl(path, context->GetSpecialStoragePolicy(),
                               quota_manager->proxy(),
                               BrowserThread::GetMessageLoopProxyForThread(
                                   BrowserThread::WEBKIT_DEPRECATED));

  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService(quota_manager->proxy());

  return new StoragePartitionImpl(partition_path,
                                  quota_manager,
                                  appcache_service,
                                  filesystem_context,
                                  database_tracker,
                                  dom_storage_context,
                                  indexed_db_context);
}

base::FilePath StoragePartitionImpl::GetPath() {
  return partition_path_;
}

net::URLRequestContextGetter* StoragePartitionImpl::GetURLRequestContext() {
  return url_request_context_;
}

net::URLRequestContextGetter*
StoragePartitionImpl::GetMediaURLRequestContext() {
  return media_url_request_context_;
}

quota::QuotaManager* StoragePartitionImpl::GetQuotaManager() {
  return quota_manager_;
}

ChromeAppCacheService* StoragePartitionImpl::GetAppCacheService() {
  return appcache_service_;
}

fileapi::FileSystemContext* StoragePartitionImpl::GetFileSystemContext() {
  return filesystem_context_;
}

webkit_database::DatabaseTracker* StoragePartitionImpl::GetDatabaseTracker() {
  return database_tracker_;
}

DOMStorageContextImpl* StoragePartitionImpl::GetDOMStorageContext() {
  return dom_storage_context_;
}

IndexedDBContextImpl* StoragePartitionImpl::GetIndexedDBContext() {
  return indexed_db_context_;
}

void StoragePartitionImpl::AsyncClearDataForOrigin(
    uint32 storage_mask,
    const GURL& storage_origin,
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ClearOriginOnIOThread,
                 storage_mask,
                 storage_origin,
                 make_scoped_refptr(request_context_getter),
                 quota_manager_));

  if (storage_mask & kLocalDomStorage)
    GetDOMStorageContext()->DeleteLocalStorage(storage_origin);
}

void StoragePartitionImpl::AsyncClearData(uint32 storage_mask) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We ignore the media request context because it shares the same cookie store
  // as the main request context.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ClearAllDataOnIOThread,
                 storage_mask,
                 url_request_context_,
                 quota_manager_));

  if (storage_mask & kLocalDomStorage) {
    dom_storage_context_->GetLocalStorageUsage(
        base::Bind(&OnLocalStorageUsageInfo, dom_storage_context_));
  }

  if (storage_mask & kSessionDomStorage) {
    dom_storage_context_->GetSessionStorageUsage(
        base::Bind(&OnSessionStorageUsageInfo, dom_storage_context_));
  }
}

void StoragePartitionImpl::SetURLRequestContext(
    net::URLRequestContextGetter* url_request_context) {
  url_request_context_ = url_request_context;
}

void StoragePartitionImpl::SetMediaURLRequestContext(
    net::URLRequestContextGetter* media_url_request_context) {
  media_url_request_context_ = media_url_request_context;
}

}  // namespace content
