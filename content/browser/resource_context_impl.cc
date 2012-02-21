// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/database/database_tracker.h"

// Key names on ResourceContext.
static const char* kAppCacheServicKeyName = "content_appcache_service_tracker";
static const char* kBlobStorageContextKeyName = "content_blob_storage_context";
static const char* kDatabaseTrackerKeyName = "content_database_tracker";
static const char* kFileSystemContextKeyName = "content_file_system_context";
static const char* kWebKitContextKeyName = "content_webkit_context";

using appcache::AppCacheService;
using base::UserDataAdapter;
using content::BrowserThread;
using fileapi::FileSystemContext;
using webkit_blob::BlobStorageController;
using webkit_database::DatabaseTracker;

namespace content {

AppCacheService* ResourceContext::GetAppCacheService(ResourceContext* context) {
  return UserDataAdapter<ChromeAppCacheService>::Get(
      context, kAppCacheServicKeyName);
}

FileSystemContext* ResourceContext::GetFileSystemContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<FileSystemContext>::Get(
      resource_context, kFileSystemContextKeyName);
}

BlobStorageController* ResourceContext::GetBlobStorageController(
    ResourceContext* resource_context) {
  return GetChromeBlobStorageContextForResourceContext(resource_context)->
      controller();
}

DatabaseTracker* GetDatabaseTrackerForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<DatabaseTracker>::Get(
      resource_context, kDatabaseTrackerKeyName);
}

WebKitContext* GetWebKitContextForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<WebKitContext>::Get(
      resource_context, kWebKitContextKeyName);
}

ChromeBlobStorageContext* GetChromeBlobStorageContextForResourceContext(
    ResourceContext* resource_context) {
  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      resource_context, kBlobStorageContextKeyName);
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();
  DCHECK(!resource_context->GetUserData(kWebKitContextKeyName));
  resource_context->SetUserData(
      kWebKitContextKeyName,
      new UserDataAdapter<WebKitContext>(
          BrowserContext::GetWebKitContext(browser_context)));
  resource_context->SetUserData(
      kDatabaseTrackerKeyName,
      new UserDataAdapter<webkit_database::DatabaseTracker>(
          BrowserContext::GetDatabaseTracker(browser_context)));
  resource_context->SetUserData(
      kAppCacheServicKeyName,
      new UserDataAdapter<ChromeAppCacheService>(
          static_cast<ChromeAppCacheService*>(
              BrowserContext::GetAppCacheService(browser_context))));
  resource_context->SetUserData(
      kFileSystemContextKeyName,
      new UserDataAdapter<FileSystemContext>(
          BrowserContext::GetFileSystemContext(browser_context)));
  resource_context->SetUserData(
      kBlobStorageContextKeyName,
      new UserDataAdapter<ChromeBlobStorageContext>(
          ChromeBlobStorageContext::GetFor(browser_context)));
}

}  // namespace content
