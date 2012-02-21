// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
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
static const char* kFileSystemContextKeyName = "content_file_system_context";
static const char* kQuotaManagerKeyName = "content_quota_manager";
static const char* kWebKitContextKeyName = "content_webkit_context";

namespace content {

void CreateQuotaManagerAndClients(BrowserContext* context) {
  if (context->GetUserData(kQuotaManagerKeyName)) {
    DCHECK(context->GetUserData(kDatabaseTrackerKeyName));
    DCHECK(context->GetUserData(kFileSystemContextKeyName));
    DCHECK(context->GetUserData(kWebKitContextKeyName));
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
      context->GetPath(), context->IsOffTheRecord(), false,
      context->GetSpecialStoragePolicy(), quota_manager->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  context->SetUserData(kDatabaseTrackerKeyName, 
                       new UserDataAdapter<DatabaseTracker>(db_tracker));

  scoped_refptr<WebKitContext> webkit_context = new WebKitContext(
      context->IsOffTheRecord(), context->GetPath(),
      context->GetSpecialStoragePolicy(), false, quota_manager->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(
          BrowserThread::WEBKIT_DEPRECATED));
  context->SetUserData(kWebKitContextKeyName,
                       new UserDataAdapter<WebKitContext>(webkit_context));

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

QuotaManager* BrowserContext::GetQuotaManager(BrowserContext* context) {
  CreateQuotaManagerAndClients(context);
  return UserDataAdapter<QuotaManager>::Get(context, kQuotaManagerKeyName);
}

WebKitContext* BrowserContext::GetWebKitContext(BrowserContext* context) {
  CreateQuotaManagerAndClients(context);
  return UserDataAdapter<WebKitContext>::Get(context, kWebKitContextKeyName);
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
  if (context->GetResourceContext()->GetUserData(kWebKitContextKeyName))
    return;
  InitializeResourceContext(context);
}

BrowserContext::~BrowserContext() {
  if (GetUserData(kDatabaseTrackerKeyName) &&
      BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&webkit_database::DatabaseTracker::Shutdown,
                   GetDatabaseTracker(this)));
  }
}

}  // namespace content
