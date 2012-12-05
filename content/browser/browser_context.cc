// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#if !defined(OS_IOS)
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/site_instance.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/database/database_tracker.h"
#endif // !OS_IOS

using base::UserDataAdapter;

namespace content {

// Only ~BrowserContext() is needed on iOS.
#if !defined(OS_IOS)
namespace {

// Key names on BrowserContext.
const char* kDownloadManagerKeyName = "download_manager";
const char* kStorageParitionMapKeyName = "content_storage_partition_map";

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionImplMap(browser_context);
    browser_context->SetUserData(kStorageParitionMapKeyName, partition_map);
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

// Run |callback| on each DOMStorageContextImpl in |browser_context|.
void PurgeDOMStorageContextInPartition(StoragePartition* storage_partition) {
  static_cast<StoragePartitionImpl*>(storage_partition)->
      GetDOMStorageContext()->PurgeMemory();
}

void SaveSessionStateOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    appcache::AppCacheService* appcache_service) {
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  context->cookie_store()->GetCookieMonster()->
      SetForceKeepSessionState();
  context->server_bound_cert_service()->GetCertStore()->
      SetForceKeepSessionState();
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnWebkitThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void PurgeMemoryOnIOThread(appcache::AppCacheService* appcache_service) {
  appcache_service->PurgeMemory();
}

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* browser_context,
    const GURL& site) {
  GetStoragePartitionMap(browser_context)->AsyncObliterate(site);
}

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
    DCHECK(rdh);
    scoped_refptr<DownloadManager> download_manager =
        new DownloadManagerImpl(
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
            storage_partition->GetAppCacheService()));
  }

  DOMStorageContextImpl* dom_storage_context_impl =
      static_cast<DOMStorageContextImpl*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_impl->SetForceKeepSessionState();

  if (BrowserThread::IsMessageLoopValid(BrowserThread::WEBKIT_DEPRECATED)) {
    IndexedDBContextImpl* indexed_db = static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
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
        base::Bind(
            &PurgeMemoryOnIOThread,
            BrowserContext::GetDefaultStoragePartition(browser_context)->
                GetAppCacheService()));
  }

  ForEachStoragePartition(browser_context,
                          base::Bind(&PurgeDOMStorageContextInPartition));
}
#endif  // !OS_IOS

BrowserContext::~BrowserContext() {
#if !defined(OS_IOS)
  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
#endif
}

}  // namespace content
