// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_data_deleter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_errors.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/database_util.h"
#include "webkit/fileapi/file_system_context.h"

ExtensionDataDeleter::ExtensionDataDeleter(Profile* profile,
                                           const GURL& extension_url) {
  DCHECK(profile);
  webkit_context_ = profile->GetWebKitContext();
  database_tracker_ = profile->GetDatabaseTracker();
  extension_request_context_ = profile->GetRequestContextForExtensions();
  file_system_context_ = profile->GetFileSystemContext();
  extension_url_ = extension_url;
  origin_id_ =
      webkit_database::DatabaseUtil::GetOriginIdentifier(extension_url_);
}

ExtensionDataDeleter::~ExtensionDataDeleter() {
}

void ExtensionDataDeleter::StartDeleting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ExtensionDataDeleter::DeleteCookiesOnIOThread));

  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
      NewRunnableMethod(
          this, &ExtensionDataDeleter::DeleteLocalStorageOnWebkitThread));

  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
      NewRunnableMethod(
          this, &ExtensionDataDeleter::DeleteIndexedDBOnWebkitThread));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ExtensionDataDeleter::DeleteDatabaseOnFileThread));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ExtensionDataDeleter::DeleteFileSystemOnFileThread));
}

void ExtensionDataDeleter::DeleteCookiesOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieMonster* cookie_monster =
      extension_request_context_->GetCookieStore()->GetCookieMonster();
  if (cookie_monster)
    cookie_monster->DeleteAllForHost(extension_url_);
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
  webkit_context_->indexed_db_context()->DeleteIndexedDBForOrigin(origin_id_);
}

void ExtensionDataDeleter::DeleteFileSystemOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  file_system_context_->DeleteDataForOriginOnFileThread(extension_url_);
}
