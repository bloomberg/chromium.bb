// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "build/build_config.h"

#if !defined(OS_IOS)
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_instance.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/external_mount_points.h"
#endif // !OS_IOS

using base::UserDataAdapter;

namespace content {

// Only ~BrowserContext() is needed on iOS.
#if !defined(OS_IOS)
namespace {

// Key names on BrowserContext.
const char kDownloadManagerKeyName[] = "download_manager";
const char kStoragePartitionMapKeyName[] = "content_storage_partition_map";

#if defined(OS_CHROMEOS)
const char kMountPointsKey[] = "mount_points";
#endif  // defined(OS_CHROMEOS)

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionImplMap(browser_context);
    browser_context->SetUserData(kStoragePartitionMapKeyName, partition_map);
  }
  return partition_map;
}

StoragePartition* GetStoragePartitionFromConfig(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory) {
  StoragePartitionImplMap* partition_map =
      GetStoragePartitionMap(browser_context);

  if (browser_context->IsOffTheRecord())
    in_memory = true;

  return partition_map->Get(partition_domain, partition_name, in_memory);
}

void SaveSessionStateOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    AppCacheServiceImpl* appcache_service) {
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  context->cookie_store()->GetCookieMonster()->
      SetForceKeepSessionState();
  context->channel_id_service()->GetChannelIDStore()->
      SetForceKeepSessionState();
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnIndexedDBThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void ShutdownServiceWorkerContext(StoragePartition* partition) {
  ServiceWorkerContextWrapper* wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  wrapper->process_manager()->Shutdown();
}

void SetDownloadManager(BrowserContext* context,
                        content::DownloadManager* download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_manager);
  context->SetUserData(kDownloadManagerKeyName, download_manager);
}

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* browser_context,
    const GURL& site,
    const base::Closure& on_gc_required) {
  GetStoragePartitionMap(browser_context)->AsyncObliterate(site,
                                                           on_gc_required);
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
      BrowserContext* browser_context,
      scoped_ptr<base::hash_set<base::FilePath> > active_paths,
      const base::Closure& done) {
  GetStoragePartitionMap(browser_context)
      ->GarbageCollect(std::move(active_paths), done);
}

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    DownloadManager* download_manager =
        new DownloadManagerImpl(
            GetContentClient()->browser()->GetNetLog(), context);

    SetDownloadManager(context, download_manager);
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
  }

  return static_cast<DownloadManager*>(
      context->GetUserData(kDownloadManagerKeyName));
}

// static
storage::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* context) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

#if defined(OS_CHROMEOS)
  if (!context->GetUserData(kMountPointsKey)) {
    scoped_refptr<storage::ExternalMountPoints> mount_points =
        storage::ExternalMountPoints::CreateRefCounted();
    context->SetUserData(
        kMountPointsKey,
        new UserDataAdapter<storage::ExternalMountPoints>(mount_points.get()));
  }

  return UserDataAdapter<storage::ExternalMountPoints>::Get(context,
                                                            kMountPointsKey);
#else
  return NULL;
#endif
}

StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;

  // TODO(ajwong): After GetDefaultStoragePartition() is removed, get rid of
  // this conditional and require that |site_instance| is non-NULL.
  if (site_instance) {
    GetContentClient()->browser()->GetStoragePartitionConfigForSite(
        browser_context, site_instance->GetSiteURL(), true,
        &partition_domain, &partition_name, &in_memory);
  }

  return GetStoragePartitionFromConfig(
      browser_context, partition_domain, partition_name, in_memory);
}

StoragePartition* BrowserContext::GetStoragePartitionForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory;

  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, site, true, &partition_domain, &partition_name,
      &in_memory);

  return GetStoragePartitionFromConfig(
      browser_context, partition_domain, partition_name, in_memory);
}

void BrowserContext::ForEachStoragePartition(
    BrowserContext* browser_context,
    const StoragePartitionCallback& callback) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(callback);
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, NULL);
}

// static
void BrowserContext::CreateMemoryBackedBlob(BrowserContext* browser_context,
                                            const char* data, size_t length,
                                            const BlobCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::CreateMemoryBackedBlob,
                 make_scoped_refptr(blob_context), data, length),
      callback);
}

// static
void BrowserContext::CreateFileBackedBlob(
    BrowserContext* browser_context,
    const base::FilePath& path,
    int64_t offset,
    int64_t size,
    const base::Time& expected_modification_time,
    const BlobCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::CreateFileBackedBlob,
                 make_scoped_refptr(blob_context), path, offset, size,
                 expected_modification_time),
      callback);
}

// static
void BrowserContext::DeliverPushMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const PushEventPayload& payload,
    const base::Callback<void(PushDeliveryStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(browser_context, origin,
                                      service_worker_registration_id, payload,
                                      callback);
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* browser_context) {
  // Service Workers must shutdown before the browser context is destroyed,
  // since they keep render process hosts alive and the codebase assumes that
  // render process hosts die before their profile (browser context) dies.
  ForEachStoragePartition(browser_context,
                          base::Bind(ShutdownServiceWorkerContext));
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
  GetDefaultStoragePartition(browser_context)->GetDatabaseTracker()->
      SetForceKeepSessionState();
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SaveSessionStateOnIOThread,
            make_scoped_refptr(browser_context->GetRequestContext()),
            static_cast<AppCacheServiceImpl*>(
                storage_partition->GetAppCacheService())));
  }

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  IndexedDBContextImpl* indexed_db_context_impl =
      static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  // No task runner in unit tests.
  if (indexed_db_context_impl->TaskRunner()) {
    indexed_db_context_impl->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SaveSessionStateOnIndexedDBThread,
                   make_scoped_refptr(indexed_db_context_impl)));
  }
}

void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* browser_context,
    DownloadManager* download_manager) {
  SetDownloadManager(browser_context, download_manager);
}

#endif  // !OS_IOS

BrowserContext::~BrowserContext() {
#if !defined(OS_IOS)
  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
#endif
}

}  // namespace content
