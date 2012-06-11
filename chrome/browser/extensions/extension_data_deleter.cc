// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_data_deleter.h"

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
#include "content/public/browser/resource_context.h"
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
using content::ResourceContext;

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

  BrowserContext::GetDOMStorageContext(profile)->DeleteOrigin(storage_origin);

  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &ExtensionDataDeleter::DeleteIndexedDBOnWebkitThread, deleter,
          make_scoped_refptr(BrowserContext::GetIndexedDBContext(profile))));

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
          &ExtensionDataDeleter::DeleteAppcachesOnIOThread, deleter,
          profile->GetResourceContext()));

  profile->GetExtensionService()->settings_frontend()->
      DeleteStorageSoon(extension_id);
}

ExtensionDataDeleter::ExtensionDataDeleter(
    Profile* profile,
    const std::string& extension_id,
    const GURL& storage_origin,
    bool is_storage_isolated)
    : extension_id_(extension_id) {
  database_tracker_ = BrowserContext::GetDatabaseTracker(profile);
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
  file_system_context_ = BrowserContext::GetFileSystemContext(profile);
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
  int rv = database_tracker_->DeleteDataForOrigin(
      origin_id_, net::CompletionCallback());
  DCHECK(rv == net::OK || rv == net::ERR_IO_PENDING);
}

void ExtensionDataDeleter::DeleteIndexedDBOnWebkitThread(
    scoped_refptr<IndexedDBContext> indexed_db_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  indexed_db_context->DeleteForOrigin(storage_origin_);
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

void ExtensionDataDeleter::DeleteAppcachesOnIOThread(ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceContext::GetAppCacheService(context)->DeleteAppCachesForOrigin(
      storage_origin_, net::CompletionCallback());
}
