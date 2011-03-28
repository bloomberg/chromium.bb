// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_indexed_db_helper.h"

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "content/browser/browser_thread.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebSecurityOrigin;

namespace {

class BrowsingDataIndexedDBHelperImpl : public BrowsingDataIndexedDBHelper {
 public:
  explicit BrowsingDataIndexedDBHelperImpl(Profile* profile);

  virtual void StartFetching(
      Callback1<const std::vector<IndexedDBInfo>& >::Type* callback);
  virtual void CancelNotification();
  virtual void DeleteIndexedDBFile(const FilePath& file_path);

 private:
  virtual ~BrowsingDataIndexedDBHelperImpl();

  // Enumerates all indexed database files in the WEBKIT thread.
  void FetchIndexedDBInfoInWebKitThread();
  // Notifies the completion callback in the UI thread.
  void NotifyInUIThread();
  // Delete a single indexed database file in the WEBKIT thread.
  void DeleteIndexedDBFileInWebKitThread(const FilePath& file_path);

  Profile* profile_;

  // This only mutates in the WEBKIT thread.
  std::vector<IndexedDBInfo> indexed_db_info_;

  // This only mutates on the UI thread.
  scoped_ptr<Callback1<const std::vector<IndexedDBInfo>& >::Type >
      completion_callback_;
  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataIndexedDBHelperImpl);
};

BrowsingDataIndexedDBHelperImpl::BrowsingDataIndexedDBHelperImpl(
    Profile* profile)
    : profile_(profile),
      completion_callback_(NULL),
      is_fetching_(false) {
  DCHECK(profile_);
}

BrowsingDataIndexedDBHelperImpl::~BrowsingDataIndexedDBHelperImpl() {
}

void BrowsingDataIndexedDBHelperImpl::StartFetching(
    Callback1<const std::vector<IndexedDBInfo>& >::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  completion_callback_.reset(callback);
  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
      NewRunnableMethod(
          this,
          &BrowsingDataIndexedDBHelperImpl::FetchIndexedDBInfoInWebKitThread));
}

void BrowsingDataIndexedDBHelperImpl::CancelNotification() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  completion_callback_.reset(NULL);
}

void BrowsingDataIndexedDBHelperImpl::DeleteIndexedDBFile(
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE,
       NewRunnableMethod(
           this,
           &BrowsingDataIndexedDBHelperImpl::
              DeleteIndexedDBFileInWebKitThread,
           file_path));
}

void BrowsingDataIndexedDBHelperImpl::FetchIndexedDBInfoInWebKitThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  file_util::FileEnumerator file_enumerator(
      profile_->GetWebKitContext()->data_path().Append(
          IndexedDBContext::kIndexedDBDirectory),
      false, file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == IndexedDBContext::kIndexedDBExtension) {
      WebSecurityOrigin web_security_origin =
          WebSecurityOrigin::createFromDatabaseIdentifier(
              webkit_glue::FilePathToWebString(file_path.BaseName()));
      if (EqualsASCII(web_security_origin.protocol(),
                      chrome::kExtensionScheme)) {
        // Extension state is not considered browsing data.
        continue;
      }
      base::PlatformFileInfo file_info;
      bool ret = file_util::GetFileInfo(file_path, &file_info);
      if (ret) {
        indexed_db_info_.push_back(IndexedDBInfo(
            web_security_origin.protocol().utf8(),
            web_security_origin.host().utf8(),
            web_security_origin.port(),
            web_security_origin.databaseIdentifier().utf8(),
            web_security_origin.toString().utf8(),
            file_path,
            file_info.size,
            file_info.last_modified));
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &BrowsingDataIndexedDBHelperImpl::NotifyInUIThread));
}

void BrowsingDataIndexedDBHelperImpl::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  // Note: completion_callback_ mutates only in the UI thread, so it's safe to
  // test it here.
  if (completion_callback_ != NULL) {
    completion_callback_->Run(indexed_db_info_);
    completion_callback_.reset();
  }
  is_fetching_ = false;
}

void BrowsingDataIndexedDBHelperImpl::DeleteIndexedDBFileInWebKitThread(
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  // TODO(jochen): implement this once it's possible to delete indexed DBs.
}

}  // namespace

BrowsingDataIndexedDBHelper::IndexedDBInfo::IndexedDBInfo(
    const std::string& protocol,
    const std::string& host,
    unsigned short port,
    const std::string& database_identifier,
    const std::string& origin,
    const FilePath& file_path,
    int64 size,
    base::Time last_modified)
    : protocol(protocol),
      host(host),
      port(port),
      database_identifier(database_identifier),
      origin(origin),
      file_path(file_path),
      size(size),
      last_modified(last_modified) {
}

BrowsingDataIndexedDBHelper::IndexedDBInfo::~IndexedDBInfo() {}

// static
BrowsingDataIndexedDBHelper* BrowsingDataIndexedDBHelper::Create(
    Profile* profile) {
  return new BrowsingDataIndexedDBHelperImpl(profile);
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::PendingIndexedDBInfo() {
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::PendingIndexedDBInfo(const GURL& origin,
                                           const string16& description)
    : origin(origin),
      description(description) {
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::~PendingIndexedDBInfo() {
}

CannedBrowsingDataIndexedDBHelper::CannedBrowsingDataIndexedDBHelper(
    Profile* profile)
    : profile_(profile),
      completion_callback_(NULL),
      is_fetching_(false) {
  DCHECK(profile);
}

CannedBrowsingDataIndexedDBHelper* CannedBrowsingDataIndexedDBHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataIndexedDBHelper* clone =
      new CannedBrowsingDataIndexedDBHelper(profile_);

  base::AutoLock auto_lock(lock_);
  clone->pending_indexed_db_info_ = pending_indexed_db_info_;
  clone->indexed_db_info_ = indexed_db_info_;
  return clone;
}

void CannedBrowsingDataIndexedDBHelper::AddIndexedDB(
    const GURL& origin, const string16& description) {
  base::AutoLock auto_lock(lock_);
  pending_indexed_db_info_.push_back(PendingIndexedDBInfo(origin, description));
}

void CannedBrowsingDataIndexedDBHelper::Reset() {
  base::AutoLock auto_lock(lock_);
  indexed_db_info_.clear();
  pending_indexed_db_info_.clear();
}

bool CannedBrowsingDataIndexedDBHelper::empty() const {
  base::AutoLock auto_lock(lock_);
  return indexed_db_info_.empty() && pending_indexed_db_info_.empty();
}

void CannedBrowsingDataIndexedDBHelper::StartFetching(
    Callback1<const std::vector<IndexedDBInfo>& >::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  completion_callback_.reset(callback);
  BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
      this,
      &CannedBrowsingDataIndexedDBHelper::ConvertPendingInfoInWebKitThread));
}

CannedBrowsingDataIndexedDBHelper::~CannedBrowsingDataIndexedDBHelper() {}

void CannedBrowsingDataIndexedDBHelper::ConvertPendingInfoInWebKitThread() {
  base::AutoLock auto_lock(lock_);
  for (std::vector<PendingIndexedDBInfo>::const_iterator
       info = pending_indexed_db_info_.begin();
       info != pending_indexed_db_info_.end(); ++info) {
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromString(
            UTF8ToUTF16(info->origin.spec()));
    std::string security_origin(web_security_origin.toString().utf8());

    bool duplicate = false;
    for (std::vector<IndexedDBInfo>::iterator
         indexed_db = indexed_db_info_.begin();
         indexed_db != indexed_db_info_.end(); ++indexed_db) {
      if (indexed_db->origin == security_origin) {
        duplicate = true;
        break;
      }
    }
    if (duplicate)
      continue;

    indexed_db_info_.push_back(IndexedDBInfo(
        web_security_origin.protocol().utf8(),
        web_security_origin.host().utf8(),
        web_security_origin.port(),
        web_security_origin.databaseIdentifier().utf8(),
        security_origin,
        profile_->GetWebKitContext()->indexed_db_context()->
            GetIndexedDBFilePath(web_security_origin.databaseIdentifier()),
        0,
        base::Time()));
  }
  pending_indexed_db_info_.clear();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &CannedBrowsingDataIndexedDBHelper::NotifyInUIThread));
}

void CannedBrowsingDataIndexedDBHelper::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  // Note: completion_callback_ mutates only in the UI thread, so it's safe to
  // test it here.
  if (completion_callback_ != NULL) {
    completion_callback_->Run(indexed_db_info_);
    completion_callback_.reset();
  }
  is_fetching_ = false;
}
