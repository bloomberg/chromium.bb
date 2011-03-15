// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_database_helper.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

using WebKit::WebSecurityOrigin;

BrowsingDataDatabaseHelper::DatabaseInfo::DatabaseInfo()
    : size(0) {
}

BrowsingDataDatabaseHelper::DatabaseInfo::DatabaseInfo(
    const std::string& host,
    const std::string& database_name,
    const std::string& origin_identifier,
    const std::string& description,
    const std::string& origin,
    int64 size,
    base::Time last_modified)
    : host(host),
      database_name(database_name),
      origin_identifier(origin_identifier),
      description(description),
      origin(origin),
      size(size),
      last_modified(last_modified) {
}

BrowsingDataDatabaseHelper::DatabaseInfo::~DatabaseInfo() {}

bool BrowsingDataDatabaseHelper::DatabaseInfo::IsFileSchemeData() {
  return StartsWithASCII(origin_identifier,
                         std::string(chrome::kFileScheme),
                         true);
}

BrowsingDataDatabaseHelper::BrowsingDataDatabaseHelper(Profile* profile)
    : completion_callback_(NULL),
      is_fetching_(false),
      tracker_(profile->GetDatabaseTracker()) {
}

BrowsingDataDatabaseHelper::~BrowsingDataDatabaseHelper() {
}

void BrowsingDataDatabaseHelper::StartFetching(
    Callback1<const std::vector<DatabaseInfo>& >::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  database_info_.clear();
  completion_callback_.reset(callback);
  BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataDatabaseHelper::FetchDatabaseInfoInWebKitThread));
}

void BrowsingDataDatabaseHelper::CancelNotification() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  completion_callback_.reset(NULL);
}

void BrowsingDataDatabaseHelper::DeleteDatabase(const std::string& origin,
                                                const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataDatabaseHelper::DeleteDatabaseInWebKitThread, origin,
      name));
}

void BrowsingDataDatabaseHelper::FetchDatabaseInfoInWebKitThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  std::vector<webkit_database::OriginInfo> origins_info;
  if (tracker_.get() && tracker_->GetAllOriginsInfo(&origins_info)) {
    for (std::vector<webkit_database::OriginInfo>::const_iterator ori =
         origins_info.begin(); ori != origins_info.end(); ++ori) {
      const std::string origin_identifier(UTF16ToUTF8(ori->GetOrigin()));
      if (StartsWithASCII(origin_identifier,
                          std::string(chrome::kExtensionScheme),
                          true)) {
        // Extension state is not considered browsing data.
        continue;
      }
      WebSecurityOrigin web_security_origin =
          WebSecurityOrigin::createFromDatabaseIdentifier(
              ori->GetOrigin());
      std::vector<string16> databases;
      ori->GetAllDatabaseNames(&databases);
      for (std::vector<string16>::const_iterator db = databases.begin();
           db != databases.end(); ++db) {
        FilePath file_path = tracker_->GetFullDBFilePath(ori->GetOrigin(), *db);
        base::PlatformFileInfo file_info;
        if (file_util::GetFileInfo(file_path, &file_info)) {
          database_info_.push_back(DatabaseInfo(
                web_security_origin.host().utf8(),
                UTF16ToUTF8(*db),
                origin_identifier,
                UTF16ToUTF8(ori->GetDatabaseDescription(*db)),
                web_security_origin.toString().utf8(),
                file_info.size,
                file_info.last_modified));
        }
      }
    }
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataDatabaseHelper::NotifyInUIThread));
}

void BrowsingDataDatabaseHelper::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  // Note: completion_callback_ mutates only in the UI thread, so it's safe to
  // test it here.
  if (completion_callback_ != NULL) {
    completion_callback_->Run(database_info_);
    completion_callback_.reset();
  }
  is_fetching_ = false;
  database_info_.clear();
}

void BrowsingDataDatabaseHelper::DeleteDatabaseInWebKitThread(
    const std::string& origin,
    const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  if (!tracker_.get())
    return;
  tracker_->DeleteDatabase(UTF8ToUTF16(origin), UTF8ToUTF16(name), NULL);
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::PendingDatabaseInfo() {}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::PendingDatabaseInfo(
    const GURL& origin,
    const std::string& name,
    const std::string& description)
    : origin(origin),
      name(name),
      description(description) {
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::~PendingDatabaseInfo() {}

CannedBrowsingDataDatabaseHelper::CannedBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile),
      profile_(profile) {
}

CannedBrowsingDataDatabaseHelper* CannedBrowsingDataDatabaseHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataDatabaseHelper* clone =
      new CannedBrowsingDataDatabaseHelper(profile_);

  base::AutoLock auto_lock(lock_);
  clone->pending_database_info_ = pending_database_info_;
  clone->database_info_ = database_info_;
  return clone;
}

void CannedBrowsingDataDatabaseHelper::AddDatabase(
    const GURL& origin,
    const std::string& name,
    const std::string& description) {
  base::AutoLock auto_lock(lock_);
  pending_database_info_.push_back(PendingDatabaseInfo(
        origin, name, description));
}

void CannedBrowsingDataDatabaseHelper::Reset() {
  base::AutoLock auto_lock(lock_);
  database_info_.clear();
  pending_database_info_.clear();
}

bool CannedBrowsingDataDatabaseHelper::empty() const {
  base::AutoLock auto_lock(lock_);
  return database_info_.empty() && pending_database_info_.empty();
}

void CannedBrowsingDataDatabaseHelper::StartFetching(
    Callback1<const std::vector<DatabaseInfo>& >::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  completion_callback_.reset(callback);
  BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
      this, &CannedBrowsingDataDatabaseHelper::ConvertInfoInWebKitThread));
}

CannedBrowsingDataDatabaseHelper::~CannedBrowsingDataDatabaseHelper() {}

void CannedBrowsingDataDatabaseHelper::ConvertInfoInWebKitThread() {
  base::AutoLock auto_lock(lock_);
  for (std::vector<PendingDatabaseInfo>::const_iterator
       info = pending_database_info_.begin();
       info != pending_database_info_.end(); ++info) {
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromString(
            UTF8ToUTF16(info->origin.spec()));
    std::string origin_identifier =
        web_security_origin.databaseIdentifier().utf8();

    bool duplicate = false;
    for (std::vector<DatabaseInfo>::iterator database = database_info_.begin();
         database != database_info_.end(); ++database) {
      if (database->origin_identifier == origin_identifier &&
          database->database_name == info->name) {
        duplicate = true;
        break;
      }
    }
    if (duplicate)
      continue;

    database_info_.push_back(DatabaseInfo(
        web_security_origin.host().utf8(),
        info->name,
        origin_identifier,
        info->description,
        web_security_origin.toString().utf8(),
        0,
        base::Time()));
  }
  pending_database_info_.clear();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
      this, &CannedBrowsingDataDatabaseHelper::NotifyInUIThread));
}
