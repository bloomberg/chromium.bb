// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "webkit/database/database_tracker.h"

using base::UserDataAdapter;

// Key names on BrowserContext.
static const char* kDownloadManagerKeyName = "download_manager";
static const char* kStorageParitionMapKeyName = "content_storage_partition_map";

namespace content {

namespace {

StoragePartition* GetStoragePartitionByPartitionId(
    BrowserContext* browser_context,
    const std::string& partition_id) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionImplMap(browser_context);
    browser_context->SetUserData(kStorageParitionMapKeyName, partition_map);
  }

  return partition_map->Get(partition_id);
}

// Run |callback| on each DOMStorageContextImpl in |browser_context|.
void PurgeDOMStorageContextInPartition(const std::string& id,
                                       StoragePartition* storage_partition) {
  static_cast<StoragePartitionImpl*>(storage_partition)->
      GetDOMStorageContext()->PurgeMemory();
}

void SaveSessionStateOnIOThread(ResourceContext* resource_context) {
  resource_context->GetRequestContext()->cookie_store()->GetCookieMonster()->
      SetForceKeepSessionState();
  resource_context->GetRequestContext()->server_bound_cert_service()->
      GetCertStore()->SetForceKeepSessionState();
  ResourceContext::GetAppCacheService(resource_context)->
      set_force_keep_session_state();
}

void SaveSessionStateOnWebkitThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void PurgeMemoryOnIOThread(ResourceContext* resource_context) {
  ResourceContext::GetAppCacheService(resource_context)->PurgeMemory();
}

}  // namespace

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
    DCHECK(rdh);
    scoped_refptr<DownloadManager> download_manager =
        new DownloadManagerImpl(
            scoped_ptr<DownloadItemFactory>(),
            scoped_ptr<DownloadFileFactory>(),
            GetContentClient()->browser()->GetNetLog());

    context->SetUserData(
        kDownloadManagerKeyName,
        new UserDataAdapter<DownloadManager>(download_manager));
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
    download_manager->Init(context);
  }

  return UserDataAdapter<DownloadManager>::Get(
      context, kDownloadManagerKeyName);
}

IndexedDBContext* BrowserContext::GetIndexedDBContext(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a SiteInstance instead of
  // using GetDefaultStoragePartition().
  return GetDefaultStoragePartition(browser_context)->GetIndexedDBContext();
}

webkit_database::DatabaseTracker* BrowserContext::GetDatabaseTracker(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a SiteInstance instead of
  // using GetDefaultStoragePartition().
  return GetDefaultStoragePartition(browser_context)->GetDatabaseTracker();
}

appcache::AppCacheService* BrowserContext::GetAppCacheService(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a SiteInstance instead of
  // using GetDefaultStoragePartition().
  return GetDefaultStoragePartition(browser_context)->GetAppCacheService();
}

fileapi::FileSystemContext* BrowserContext::GetFileSystemContext(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a SiteInstance instead of
  // using GetDefaultStoragePartition().
  return GetDefaultStoragePartition(browser_context)->GetFileSystemContext();
}

StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  std::string partition_id;  // Default to "" for NULL |site_instance|.

  // TODO(ajwong): After GetDefaultStoragePartition() is removed, get rid of
  // this conditional and require that |site_instance| is non-NULL.
  if (site_instance) {
    partition_id = GetContentClient()->browser()->
        GetStoragePartitionIdForSiteInstance(browser_context,
                                             site_instance);
  }

  return GetStoragePartitionByPartitionId(browser_context, partition_id);
}

void BrowserContext::ForEachStoragePartition(
    BrowserContext* browser_context,
    const StoragePartitionCallback& callback) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(callback);
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, NULL);
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext() directly here is that
  // ResourceContext initialization may call back into BrowserContext
  // and when that call returns it'll end rewriting its UserData map. It will
  // end up rewriting the same value but this still causes a race condition.
  //
  // See http://crbug.com/115678.
  GetDefaultStoragePartition(context);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  GetDatabaseTracker(browser_context)->SetForceKeepSessionState();

  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SaveSessionStateOnIOThread,
                   browser_context->GetResourceContext()));
  }

  DOMStorageContextImpl* dom_storage_context_impl =
      static_cast<DOMStorageContextImpl*>(
          GetDefaultStoragePartition(browser_context)->GetDOMStorageContext());
  dom_storage_context_impl->SetForceKeepSessionState();

  if (BrowserThread::IsMessageLoopValid(BrowserThread::WEBKIT_DEPRECATED)) {
    IndexedDBContextImpl* indexed_db = static_cast<IndexedDBContextImpl*>(
        GetIndexedDBContext(browser_context));
    BrowserThread::PostTask(
        BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
        base::Bind(&SaveSessionStateOnWebkitThread,
                   make_scoped_refptr(indexed_db)));
  }
}

void BrowserContext::PurgeMemory(BrowserContext* browser_context) {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PurgeMemoryOnIOThread,
                   browser_context->GetResourceContext()));
  }

  ForEachStoragePartition(browser_context,
                          base::Bind(&PurgeDOMStorageContextInPartition));
}

BrowserContext::~BrowserContext() {
  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
}

}  // namespace content
