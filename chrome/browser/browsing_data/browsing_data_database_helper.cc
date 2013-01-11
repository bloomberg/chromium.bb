// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_database_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

using content::BrowserContext;
using content::BrowserThread;
using WebKit::WebSecurityOrigin;

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

BrowsingDataDatabaseHelper::BrowsingDataDatabaseHelper(Profile* profile)
    : is_fetching_(false),
      tracker_(BrowserContext::
                  GetDefaultStoragePartition(profile)->GetDatabaseTracker()) {
}

BrowsingDataDatabaseHelper::~BrowsingDataDatabaseHelper() {
}

void BrowsingDataDatabaseHelper::StartFetching(
    const base::Callback<void(const std::list<DatabaseInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  database_info_.clear();
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BrowsingDataDatabaseHelper::FetchDatabaseInfoOnFileThread,
                 this));
}

void BrowsingDataDatabaseHelper::DeleteDatabase(const std::string& origin,
                                                const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BrowsingDataDatabaseHelper::DeleteDatabaseOnFileThread, this,
                 origin, name));
}

void BrowsingDataDatabaseHelper::FetchDatabaseInfoOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<webkit_database::OriginInfo> origins_info;
  if (tracker_.get() && tracker_->GetAllOriginsInfo(&origins_info)) {
    for (std::vector<webkit_database::OriginInfo>::const_iterator ori =
         origins_info.begin(); ori != origins_info.end(); ++ori) {
      WebSecurityOrigin web_security_origin =
          WebSecurityOrigin::createFromDatabaseIdentifier(ori->GetOrigin());
      GURL origin_url(web_security_origin.toString().utf8());
      if (!BrowsingDataHelper::HasWebScheme(origin_url)) {
        // Non-websafe state is not considered browsing data.
        continue;
      }
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
                UTF16ToUTF8(ori->GetOrigin()),
                UTF16ToUTF8(ori->GetDatabaseDescription(*db)),
                origin_url.spec(),
                file_info.size,
                file_info.last_modified));
        }
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataDatabaseHelper::NotifyInUIThread, this));
}

void BrowsingDataDatabaseHelper::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  completion_callback_.Run(database_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
  database_info_.clear();
}

void BrowsingDataDatabaseHelper::DeleteDatabaseOnFileThread(
    const std::string& origin,
    const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!tracker_.get())
    return;
  tracker_->DeleteDatabase(UTF8ToUTF16(origin), UTF8ToUTF16(name),
                           net::CompletionCallback());
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::PendingDatabaseInfo(
    const GURL& origin,
    const std::string& name,
    const std::string& description)
    : origin(origin),
      name(name),
      description(description) {
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::~PendingDatabaseInfo() {}

bool CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::operator<(
    const PendingDatabaseInfo& other) const {
  if (origin == other.origin)
    return name < other.name;
  return origin < other.origin;
}

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
  return clone;
}

void CannedBrowsingDataDatabaseHelper::AddDatabase(
    const GURL& origin,
    const std::string& name,
    const std::string& description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
  if (BrowsingDataHelper::HasWebScheme(origin)) {
    pending_database_info_.insert(PendingDatabaseInfo(
          origin, name, description));
  }
}

void CannedBrowsingDataDatabaseHelper::Reset() {
  base::AutoLock auto_lock(lock_);
  pending_database_info_.clear();
}

bool CannedBrowsingDataDatabaseHelper::empty() const {
  base::AutoLock auto_lock(lock_);
  return pending_database_info_.empty();
}

size_t CannedBrowsingDataDatabaseHelper::GetDatabaseCount() const {
  base::AutoLock auto_lock(lock_);
  return pending_database_info_.size();
}

const std::set<CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo>&
CannedBrowsingDataDatabaseHelper::GetPendingDatabaseInfo() {
  return pending_database_info_;
}

void CannedBrowsingDataDatabaseHelper::StartFetching(
    const base::Callback<void(const std::list<DatabaseInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(&CannedBrowsingDataDatabaseHelper::ConvertInfoInWebKitThread,
                 this));
}

CannedBrowsingDataDatabaseHelper::~CannedBrowsingDataDatabaseHelper() {}

void CannedBrowsingDataDatabaseHelper::ConvertInfoInWebKitThread() {
  base::AutoLock auto_lock(lock_);
  database_info_.clear();
  for (std::set<PendingDatabaseInfo>::const_iterator
       info = pending_database_info_.begin();
       info != pending_database_info_.end(); ++info) {
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromString(
            UTF8ToUTF16(info->origin.spec()));
    std::string origin_identifier =
        web_security_origin.databaseIdentifier().utf8();

    database_info_.push_back(DatabaseInfo(
        web_security_origin.host().utf8(),
        info->name,
        origin_identifier,
        info->description,
        web_security_origin.toString().utf8(),
        0,
        base::Time()));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CannedBrowsingDataDatabaseHelper::NotifyInUIThread, this));
}
