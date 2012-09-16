// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/data_deleter.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"
#include "webkit/fileapi/file_system_context.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;
using content::IndexedDBContext;

namespace extensions {

// static
void DataDeleter::StartDeleting(Profile* profile,
                                const std::string& extension_id,
                                const GURL& storage_origin,
                                bool is_storage_isolated) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  scoped_refptr<DataDeleter> deleter = new DataDeleter(
      profile, extension_id, storage_origin, is_storage_isolated);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DataDeleter::DeleteCookiesOnIOThread, deleter));

  content::BrowserContext::GetDefaultStoragePartition(profile)->
      GetDOMStorageContext()->DeleteOrigin(storage_origin);

  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &DataDeleter::DeleteIndexedDBOnWebkitThread,
          deleter));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DataDeleter::DeleteDatabaseOnFileThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DataDeleter::DeleteFileSystemOnFileThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DataDeleter::DeleteAppcachesOnIOThread,
                 deleter,
                 BrowserContext::GetDefaultStoragePartition(profile)->
                     GetAppCacheService()));

  profile->GetExtensionService()->settings_frontend()->
      DeleteStorageSoon(extension_id);
}

DataDeleter::DataDeleter(
    Profile* profile,
    const std::string& extension_id,
    const GURL& storage_origin,
    bool is_storage_isolated)
    : extension_id_(extension_id) {
  // TODO(michaeln): Delete from the right StoragePartition.
  // http://crbug.com/85127
  database_tracker_ = BrowserContext::GetDefaultStoragePartition(profile)->
      GetDatabaseTracker();
  // Pick the right request context depending on whether it's an extension,
  // isolated app, or regular app.
  content::StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(profile);
  if (storage_origin.SchemeIs(chrome::kExtensionScheme)) {
    extension_request_context_ = profile->GetRequestContextForExtensions();
  } else if (is_storage_isolated) {
    extension_request_context_ =
        profile->GetRequestContextForIsolatedApp(extension_id);
    isolated_app_path_ =
        profile->GetPath().Append(
            content::StoragePartition::GetPartitionPath(extension_id));
  } else {
    extension_request_context_ = profile->GetRequestContext();
  }

  file_system_context_ = BrowserContext::GetFileSystemContext(profile);
  indexed_db_context_ = storage_partition->GetIndexedDBContext();

  storage_origin_ = storage_origin;
  origin_id_ =
      webkit_database::DatabaseUtil::GetOriginIdentifier(storage_origin_);
}

DataDeleter::~DataDeleter() {
}

void DataDeleter::DeleteCookiesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieMonster* cookie_monster =
      extension_request_context_->GetURLRequestContext()->cookie_store()->
          GetCookieMonster();
  if (cookie_monster)
    cookie_monster->DeleteAllForHostAsync(
        storage_origin_, net::CookieMonster::DeleteCallback());
}

void DataDeleter::DeleteDatabaseOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int rv = database_tracker_->DeleteDataForOrigin(
      origin_id_, net::CompletionCallback());
  DCHECK(rv == net::OK || rv == net::ERR_IO_PENDING);
}

void DataDeleter::DeleteIndexedDBOnWebkitThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  indexed_db_context_->DeleteForOrigin(storage_origin_);
}

void DataDeleter::DeleteFileSystemOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  file_system_context_->DeleteDataForOriginOnFileThread(storage_origin_);

  // TODO(creis): The following call fails because the request context is still
  // around, and holding open file handles in this directory.
  // See http://crbug.com/85127
  if (!isolated_app_path_.empty())
    file_util::Delete(isolated_app_path_, true);
}

void DataDeleter::DeleteAppcachesOnIOThread(
    appcache::AppCacheService* appcache_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  appcache_service->DeleteAppCachesForOrigin(storage_origin_,
                                             net::CompletionCallback());
}

}  // namespace extensions
