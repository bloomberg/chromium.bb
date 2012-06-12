// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_constants.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

using appcache::AppCacheService;
using base::UserDataAdapter;
using content::BrowserThread;
using fileapi::FileSystemContext;
using quota::QuotaManager;
using webkit_database::DatabaseTracker;

// Key names on BrowserContext.
static const char* kAppCacheServicKeyName = "content_appcache_service_tracker";
static const char* kDatabaseTrackerKeyName = "content_database_tracker";
static const char* kDOMStorageContextKeyName = "content_dom_storage_context";
static const char* kDownloadManagerKeyName = "download_manager";
static const char* kFileSystemContextKeyName = "content_file_system_context";
static const char* kIndexedDBContextKeyName = "content_indexed_db_context";
static const char* kQuotaManagerKeyName = "content_quota_manager";

namespace content {

namespace {

void CreateQuotaManagerAndClients(BrowserContext* context) {
  if (context->GetUserData(kQuotaManagerKeyName)) {
    DCHECK(context->GetUserData(kDatabaseTrackerKeyName));
    DCHECK(context->GetUserData(kDOMStorageContextKeyName));
    DCHECK(context->GetUserData(kFileSystemContextKeyName));
    DCHECK(context->GetUserData(kIndexedDBContextKeyName));
    return;
  }

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManger being used. So we do them
  // all together here prior to handing out a reference to anything
  // that utlizes the QuotaManager.
  scoped_refptr<QuotaManager> quota_manager = new quota::QuotaManager(
    context->IsOffTheRecord(), context->GetPath(),
    BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
    BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
    context->GetSpecialStoragePolicy());
  context->SetUserData(kQuotaManagerKeyName,
                       new UserDataAdapter<QuotaManager>(quota_manager));

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  scoped_refptr<FileSystemContext> filesystem_context = CreateFileSystemContext(
      context->GetPath(), context->IsOffTheRecord(),
      context->GetSpecialStoragePolicy(), quota_manager->proxy());
  context->SetUserData(
      kFileSystemContextKeyName,
      new UserDataAdapter<FileSystemContext>(filesystem_context));

  scoped_refptr<DatabaseTracker> db_tracker = new DatabaseTracker(
      context->GetPath(), context->IsOffTheRecord(),
      context->GetSpecialStoragePolicy(), quota_manager->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  context->SetUserData(kDatabaseTrackerKeyName,
                       new UserDataAdapter<DatabaseTracker>(db_tracker));

  FilePath path = context->IsOffTheRecord() ? FilePath() : context->GetPath();
  scoped_refptr<DOMStorageContextImpl> dom_storage_context =
      new DOMStorageContextImpl(path, context->GetSpecialStoragePolicy());
  context->SetUserData(
      kDOMStorageContextKeyName,
      new UserDataAdapter<DOMStorageContextImpl>(dom_storage_context));

  scoped_refptr<IndexedDBContext> indexed_db_context = new IndexedDBContextImpl(
      path, context->GetSpecialStoragePolicy(), quota_manager->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(
          BrowserThread::WEBKIT_DEPRECATED));
  context->SetUserData(
      kIndexedDBContextKeyName,
      new UserDataAdapter<IndexedDBContext>(indexed_db_context));

  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService(quota_manager->proxy());
  context->SetUserData(
      kAppCacheServicKeyName,
      new UserDataAdapter<ChromeAppCacheService>(appcache_service));

  InitializeResourceContext(context);

  // Check first to avoid memory leak in unittests.
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
                   appcache_service,
                   context->IsOffTheRecord() ? FilePath() :
                       context->GetPath().Append(content::kAppCacheDirname),
                   context->GetResourceContext(),
                   make_scoped_refptr(context->GetSpecialStoragePolicy())));
  }
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

DOMStorageContextImpl* GetDOMStorageContextImpl(BrowserContext* context) {
  return static_cast<DOMStorageContextImpl*>(
      BrowserContext::GetDOMStorageContext(context));
}

}  // namespace

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
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

QuotaManager* BrowserContext::GetQuotaManager(BrowserContext* context) {
  CreateQuotaManagerAndClients(context);
  return UserDataAdapter<QuotaManager>::Get(context, kQuotaManagerKeyName);
}

DOMStorageContext* BrowserContext::GetDOMStorageContext(
    BrowserContext* context) {
  CreateQuotaManagerAndClients(context);
  return UserDataAdapter<DOMStorageContextImpl>::Get(
      context, kDOMStorageContextKeyName);
}

IndexedDBContext* BrowserContext::GetIndexedDBContext(BrowserContext* context) {
  CreateQuotaManagerAndClients(context);
  return UserDataAdapter<IndexedDBContext>::Get(
      context, kIndexedDBContextKeyName);
}

DatabaseTracker* BrowserContext::GetDatabaseTracker(BrowserContext* context) {
  CreateQuotaManagerAndClients(context);
  return UserDataAdapter<DatabaseTracker>::Get(
      context, kDatabaseTrackerKeyName);
}

AppCacheService* BrowserContext::GetAppCacheService(
    BrowserContext* browser_context) {
  CreateQuotaManagerAndClients(browser_context);
  return UserDataAdapter<ChromeAppCacheService>::Get(
      browser_context, kAppCacheServicKeyName);
}

FileSystemContext* BrowserContext::GetFileSystemContext(
    BrowserContext* browser_context) {
  CreateQuotaManagerAndClients(browser_context);
  return UserDataAdapter<FileSystemContext>::Get(
      browser_context, kFileSystemContextKeyName);
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext directly here is that if
  // ResourceContext ends up initializing it will call back into BrowserContext
  // and when that call return it'll end rewriting its UserData map (with the
  // same value) but this causes a race condition. See http://crbug.com/115678.
  CreateQuotaManagerAndClients(context);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  GetDatabaseTracker(browser_context)->SetForceKeepSessionState();

  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SaveSessionStateOnIOThread,
                   browser_context->GetResourceContext()));
  }

  GetDOMStorageContextImpl(browser_context)->SetForceKeepSessionState();

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

  GetDOMStorageContextImpl(browser_context)->PurgeMemory();
}

BrowserContext::~BrowserContext() {
  // These message loop checks are just to avoid leaks in unittests.
  if (GetUserData(kDatabaseTrackerKeyName) &&
      BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&webkit_database::DatabaseTracker::Shutdown,
                   GetDatabaseTracker(this)));
  }

  if (GetUserData(kDOMStorageContextKeyName))
    GetDOMStorageContextImpl(this)->Shutdown();

  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
}

}  // namespace content
