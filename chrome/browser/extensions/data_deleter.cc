// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/data_deleter.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"
#include "webkit/fileapi/file_system_context.h"

using content::BrowserContext;
using content::BrowserThread;
using content::IndexedDBContext;

namespace extensions {

namespace {

void HandleIOThreadContexts(const GURL& storage_origin,
                            net::URLRequestContextGetter* request_context,
                            appcache::AppCacheService* appcache_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Handle the cookies.
  net::CookieMonster* cookie_monster =
      request_context->GetURLRequestContext()->cookie_store()->
          GetCookieMonster();
  if (cookie_monster)
    cookie_monster->DeleteAllForHostAsync(
        storage_origin, net::CookieMonster::DeleteCallback());

  // Clear out appcache.
  appcache_service->DeleteAppCachesForOrigin(storage_origin,
                                             net::CompletionCallback());
}

void HandleFileThreadContexts(
    const GURL& storage_origin,
    string16 origin_id,
    webkit_database::DatabaseTracker* database_tracker,
    fileapi::FileSystemContext* file_system_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Clear out the HTML5 filesystem.
  file_system_context->DeleteDataForOriginOnFileThread(storage_origin);

  // Clear out the database tracker.  We just let this run until completion
  // without notification.
  int rv = database_tracker->DeleteDataForOrigin(
      origin_id, net::CompletionCallback());
  DCHECK(rv == net::OK || rv == net::ERR_IO_PENDING);
}

}  // namespace

// static
void DataDeleter::StartDeleting(Profile* profile,
                                const std::string& extension_id,
                                const GURL& storage_origin,
                                bool is_storage_isolated) {
  // TODO(ajwong): If |is_storage_isolated|, we should just blowaway the
  // whole directory that the associated StoragePartition is located at. To do
  // this, we need to ensure that all contexts referencing that directory have
  // closed their file handles, otherwise Windows will complain.
  //
  // http://www.crbug.com/85127
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);

  const GURL& url = Extension::GetBaseURLFromExtensionId(extension_id);
  content::StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(profile, url);
  string16 origin_id =
      webkit_database::DatabaseUtil::GetOriginIdentifier(storage_origin);

  scoped_refptr<net::URLRequestContextGetter> request_context;
  if (storage_origin.SchemeIs(extensions::kExtensionScheme)) {
    // TODO(ajwong): Cookies are not properly isolated for
    // chrome-extension:// scheme.  (http://crbug.com/158386).
    //
    // However, no isolated apps actually can write to kExtensionScheme
    // origins. Thus, it is benign to delete from the
    // RequestContextForExtensions because there's nothing stored there. We
    // preserve this code path without checking for isolation because it's
    // simpler than special casing.  This code should go away once we merge
    // the various URLRequestContexts (http://crbug.com/159193).
    request_context = profile->GetRequestContextForExtensions();
  } else {
    // We don't need to worry about the media request context because that
    // shares the same cookie store as the main request context.
    request_context = partition->GetURLRequestContext();
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&HandleIOThreadContexts,
                 storage_origin,
                 request_context,
                 partition->GetAppCacheService()));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&HandleFileThreadContexts,
                 storage_origin,
                 origin_id,
                 make_scoped_refptr(partition->GetDatabaseTracker()),
                 make_scoped_refptr(partition->GetFileSystemContext())));

  partition->GetDOMStorageContext()->DeleteLocalStorage(storage_origin);

  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &IndexedDBContext::DeleteForOrigin,
          make_scoped_refptr(partition->GetIndexedDBContext()),
          storage_origin));

  // Begin removal of the settings for the current extension.
  profile->GetExtensionService()->settings_frontend()->
      DeleteStorageSoon(extension_id);
}
}  // namespace extensions
