// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_data_deleter.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"
#include "webkit/fileapi/file_system_context.h"

using content::BrowserThread;

// static
void ExtensionDataDeleter::StartDeleting(
    Profile* profile,
    const std::string& extension_id,
    const GURL& storage_origin,
    bool is_storage_isolated) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  scoped_refptr<ExtensionDataDeleter> deleter =
      new ExtensionDataDeleter(
          profile, extension_id, storage_origin, is_storage_isolated);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteCookiesOnIOThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteLocalStorageOnWebkitThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteIndexedDBOnWebkitThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteDatabaseOnFileThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteFileSystemOnFileThread, deleter));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteAppcachesOnIOThread, deleter));

  profile->GetExtensionService()->settings_frontend()->
      DeleteStorageSoon(extension_id);
}

ExtensionDataDeleter::ExtensionDataDeleter(
    Profile* profile,
    const std::string& extension_id,
    const GURL& storage_origin,
    bool is_storage_isolated)
    : extension_id_(extension_id) {
  appcache_service_ = profile->GetAppCacheService();
  webkit_context_ = profile->GetWebKitContext();
  database_tracker_ = profile->GetDatabaseTracker();
  // Pick the right request context depending on whether it's an extension,
  // isolated app, or regular app.
  if (storage_origin.SchemeIs(chrome::kExtensionScheme)) {
    extension_request_context_ = profile->GetRequestContextForExtensions();
  } else if (is_storage_isolated) {
    extension_request_context_ =
        profile->GetRequestContextForIsolatedApp(extension_id);
    isolated_app_path_ = profile->GetPath().
        Append(chrome::kIsolatedAppStateDirname).AppendASCII(extension_id);
  } else {
    extension_request_context_ = profile->GetRequestContext();
  }
  file_system_context_ = profile->GetFileSystemContext();
  storage_origin_ = storage_origin;
  origin_id_ =
      webkit_database::DatabaseUtil::GetOriginIdentifier(storage_origin_);
}

ExtensionDataDeleter::~ExtensionDataDeleter() {
}

void ExtensionDataDeleter::DeleteCookiesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieMonster* cookie_monster =
      extension_request_context_->GetURLRequestContext()->cookie_store()->
          GetCookieMonster();
  if (cookie_monster)
    cookie_monster->DeleteAllForHostAsync(
        storage_origin_, net::CookieMonster::DeleteCallback());
}

void ExtensionDataDeleter::DeleteDatabaseOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int rv = database_tracker_->DeleteDataForOrigin(origin_id_, NULL);
  DCHECK(rv == net::OK || rv == net::ERR_IO_PENDING);
}

void ExtensionDataDeleter::DeleteLocalStorageOnWebkitThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  webkit_context_->dom_storage_context()->DeleteLocalStorageForOrigin(
      origin_id_);
}

void ExtensionDataDeleter::DeleteIndexedDBOnWebkitThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  webkit_context_->indexed_db_context()->DeleteIndexedDBForOrigin(
      storage_origin_);
}

void ExtensionDataDeleter::DeleteFileSystemOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  file_system_context_->DeleteDataForOriginOnFileThread(storage_origin_);

  // TODO(creis): The following call fails because the request context is still
  // around, and holding open file handles in this directory.
  // See http://crbug.com/85127
  if (!isolated_app_path_.empty())
    file_util::Delete(isolated_app_path_, true);
}

void ExtensionDataDeleter::DeleteAppcachesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  appcache_service_->DeleteAppCachesForOrigin(
      storage_origin_, net::CompletionCallback());
}
