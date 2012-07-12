// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "webkit/database/database_tracker.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_context.h"
#include "content/browser/storage_partition.h"
#include "content/browser/storage_partition_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"

using base::UserDataAdapter;

// Key names on BrowserContext.
static const char* kDownloadManagerKeyName = "download_manager";
static const char* kStorageParitionMapKeyName = "content_storage_partition_map";

namespace content {

namespace {

StoragePartition* GetStoragePartition(BrowserContext* browser_context,
                                      int renderer_child_id) {
  StoragePartitionMap* partition_map = static_cast<StoragePartitionMap*>(
      browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionMap(browser_context);
    browser_context->SetUserData(kStorageParitionMapKeyName, partition_map);
  }

  const std::string& partition_id =
      GetContentClient()->browser()->GetStoragePartitionIdForChildProcess(
          browser_context,
          renderer_child_id);

  return partition_map->Get(partition_id);
}

// Run |callback| on each storage partition in |browser_context|.
void ForEachStoragePartition(
    BrowserContext* browser_context,
    const base::Callback<void(StoragePartition*)>& callback) {
  StoragePartitionMap* partition_map = static_cast<StoragePartitionMap*>(
      browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map) {
    return;
  }

  partition_map->ForEach(callback);
}

// Used to convert a callback meant to take a DOMStorageContextImpl* into one
// that can take a StoragePartition*.
void ProcessDOMStorageContext(
    const base::Callback<void(DOMStorageContextImpl*)>& callback,
    StoragePartition* partition) {
  callback.Run(partition->dom_storage_context());
}

// Run |callback| on each DOMStorageContextImpl in |browser_context|.
void ForEachDOMStorageContext(
      BrowserContext* browser_context,
      const base::Callback<void(DOMStorageContextImpl*)>& callback) {
  ForEachStoragePartition(browser_context,
                          base::Bind(&ProcessDOMStorageContext, callback));
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

DOMStorageContextImpl* GetDefaultDOMStorageContextImpl(
    BrowserContext* context) {
  return static_cast<DOMStorageContextImpl*>(
      BrowserContext::GetDefaultDOMStorageContext(context));
}

}  // namespace

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
    DCHECK(rdh);
    DownloadFileManager* file_manager = rdh->download_file_manager();
    DCHECK(file_manager);
    scoped_refptr<DownloadManager> download_manager =
        new DownloadManagerImpl(
            file_manager,
            scoped_ptr<DownloadItemFactory>(),
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

quota::QuotaManager* BrowserContext::GetQuotaManager(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a process id instead of using
  // kInvalidChildProcessId.
  StoragePartition* partition =
      GetStoragePartition(browser_context,
                          ChildProcessHostImpl::kInvalidChildProcessId);
  return partition->quota_manager();
}

DOMStorageContext* BrowserContext::GetDefaultDOMStorageContext(
    BrowserContext* browser_context) {
  // TODO(ajwong): Force all users to know which process id they are performing
  // actions on behalf of, migrate them to GetDOMStorageContext(), and then
  // delete this function.
  return GetDOMStorageContext(browser_context,
                              ChildProcessHostImpl::kInvalidChildProcessId);
}

DOMStorageContext* BrowserContext::GetDOMStorageContext(
    BrowserContext* browser_context,
    int render_child_id) {
  StoragePartition* partition =
      GetStoragePartition(browser_context, render_child_id);
  return partition->dom_storage_context();
}

IndexedDBContext* BrowserContext::GetIndexedDBContext(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a process id instead of using
  // kInvalidChildProcessId.
  StoragePartition* partition =
      GetStoragePartition(browser_context,
                          ChildProcessHostImpl::kInvalidChildProcessId);
  return partition->indexed_db_context();
}

webkit_database::DatabaseTracker* BrowserContext::GetDatabaseTracker(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a process id instead of using
  // kInvalidChildProcessId.
  StoragePartition* partition =
      GetStoragePartition(browser_context,
                          ChildProcessHostImpl::kInvalidChildProcessId);
  return partition->database_tracker();
}

appcache::AppCacheService* BrowserContext::GetAppCacheService(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a process id instead of using
  // kInvalidChildProcessId.
  StoragePartition* partition =
      GetStoragePartition(browser_context,
                          ChildProcessHostImpl::kInvalidChildProcessId);
  return partition->appcache_service();
}

fileapi::FileSystemContext* BrowserContext::GetFileSystemContext(
    BrowserContext* browser_context) {
  // TODO(ajwong): Change this API to require a process id instead of using
  // kInvalidChildProcessId.
  StoragePartition* partition =
      GetStoragePartition(browser_context,
                          ChildProcessHostImpl::kInvalidChildProcessId);
  return partition->filesystem_context();
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext directly here is that if
  // ResourceContext ends up initializing it will call back into BrowserContext
  // and when that call returns it'll end rewriting its UserData map (with the
  // same value) but this causes a race condition. See http://crbug.com/115678.
  GetStoragePartition(context, ChildProcessHostImpl::kInvalidChildProcessId);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  GetDatabaseTracker(browser_context)->SetForceKeepSessionState();

  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SaveSessionStateOnIOThread,
                   browser_context->GetResourceContext()));
  }

  // TODO(ajwong): This is the only usage of GetDefaultDOMStorageContextImpl().
  // After we migrate this to support multiple DOMStorageContexts, don't forget
  // to remove the GetDefaultDOMStorageContextImpl() function as well.
  GetDefaultDOMStorageContextImpl(browser_context)->SetForceKeepSessionState();

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

  ForEachDOMStorageContext(browser_context,
                           base::Bind(&DOMStorageContextImpl::PurgeMemory));
}

BrowserContext::~BrowserContext() {
  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
}

}  // namespace content
