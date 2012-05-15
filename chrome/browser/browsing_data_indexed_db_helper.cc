// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_indexed_db_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"
#include "webkit/database/database_util.h"
#include "webkit/glue/webkit_glue.h"

using content::BrowserContext;
using content::BrowserThread;
using content::IndexedDBContext;
using webkit_database::DatabaseUtil;

namespace {

class BrowsingDataIndexedDBHelperImpl : public BrowsingDataIndexedDBHelper {
 public:
  explicit BrowsingDataIndexedDBHelperImpl(Profile* profile);

  virtual void StartFetching(
      const base::Callback<void(const std::list<IndexedDBInfo>&)>&
          callback) OVERRIDE;
  virtual void DeleteIndexedDB(const GURL& origin) OVERRIDE;

 private:
  virtual ~BrowsingDataIndexedDBHelperImpl();

  // Enumerates all indexed database files in the WEBKIT thread.
  void FetchIndexedDBInfoInWebKitThread();
  // Notifies the completion callback in the UI thread.
  void NotifyInUIThread();
  // Delete a single indexed database in the WEBKIT thread.
  void DeleteIndexedDBInWebKitThread(const GURL& origin);

  scoped_refptr<IndexedDBContext> indexed_db_context_;

  // Access to |indexed_db_info_| is triggered indirectly via the UI thread and
  // guarded by |is_fetching_|. This means |indexed_db_info_| is only accessed
  // while |is_fetching_| is true. The flag |is_fetching_| is only accessed on
  // the UI thread.
  // In the context of this class |indexed_db_info_| is only accessed on the
  // WEBKIT thread.
  std::list<IndexedDBInfo> indexed_db_info_;

  // This only mutates on the UI thread.
  base::Callback<void(const std::list<IndexedDBInfo>&)> completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataIndexedDBHelperImpl);
};

BrowsingDataIndexedDBHelperImpl::BrowsingDataIndexedDBHelperImpl(
    Profile* profile)
    : indexed_db_context_(BrowserContext::GetIndexedDBContext(profile)),
      is_fetching_(false) {
  DCHECK(indexed_db_context_.get());
}

BrowsingDataIndexedDBHelperImpl::~BrowsingDataIndexedDBHelperImpl() {
}

void BrowsingDataIndexedDBHelperImpl::StartFetching(
    const base::Callback<void(const std::list<IndexedDBInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &BrowsingDataIndexedDBHelperImpl::FetchIndexedDBInfoInWebKitThread,
          this));
}

void BrowsingDataIndexedDBHelperImpl::DeleteIndexedDB(
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &BrowsingDataIndexedDBHelperImpl::DeleteIndexedDBInWebKitThread, this,
          origin));
}

void BrowsingDataIndexedDBHelperImpl::FetchIndexedDBInfoInWebKitThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  std::vector<GURL> origins = indexed_db_context_->GetAllOrigins();
  for (std::vector<GURL>::const_iterator iter = origins.begin();
       iter != origins.end(); ++iter) {
    const GURL& origin = *iter;
    if (!BrowsingDataHelper::HasValidScheme(origin))
      continue;  // Non-websafe state is not considered browsing data.

    indexed_db_info_.push_back(IndexedDBInfo(
        origin,
        indexed_db_context_->GetOriginDiskUsage(origin),
        indexed_db_context_->GetOriginLastModified(origin)));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataIndexedDBHelperImpl::NotifyInUIThread, this));
}

void BrowsingDataIndexedDBHelperImpl::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  completion_callback_.Run(indexed_db_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
}

void BrowsingDataIndexedDBHelperImpl::DeleteIndexedDBInWebKitThread(
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  indexed_db_context_->DeleteForOrigin(origin);
}

}  // namespace

BrowsingDataIndexedDBHelper::IndexedDBInfo::IndexedDBInfo(
    const GURL& origin,
    int64 size,
    base::Time last_modified)
    : origin(origin),
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
PendingIndexedDBInfo::PendingIndexedDBInfo(const GURL& origin,
                                           const string16& name)
    : origin(origin),
      name(name) {
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::~PendingIndexedDBInfo() {
}

bool CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo::operator<(
    const PendingIndexedDBInfo& other) const {
  if (origin == other.origin)
    return name < other.name;
  return origin < other.origin;
}

CannedBrowsingDataIndexedDBHelper::CannedBrowsingDataIndexedDBHelper()
    : is_fetching_(false) {
}

CannedBrowsingDataIndexedDBHelper::~CannedBrowsingDataIndexedDBHelper() {}

CannedBrowsingDataIndexedDBHelper* CannedBrowsingDataIndexedDBHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataIndexedDBHelper* clone =
      new CannedBrowsingDataIndexedDBHelper();

  base::AutoLock auto_lock(lock_);
  clone->pending_indexed_db_info_ = pending_indexed_db_info_;
  clone->indexed_db_info_ = indexed_db_info_;
  return clone;
}

void CannedBrowsingDataIndexedDBHelper::AddIndexedDB(
    const GURL& origin, const string16& name) {
  if (!BrowsingDataHelper::HasValidScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  base::AutoLock auto_lock(lock_);
  pending_indexed_db_info_.insert(PendingIndexedDBInfo(origin, name));
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

size_t CannedBrowsingDataIndexedDBHelper::GetIndexedDBCount() const {
  base::AutoLock auto_lock(lock_);
  return pending_indexed_db_info_.size();
}

const std::set<CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo>&
CannedBrowsingDataIndexedDBHelper::GetIndexedDBInfo() const  {
  base::AutoLock auto_lock(lock_);
  return pending_indexed_db_info_;
}

void CannedBrowsingDataIndexedDBHelper::StartFetching(
    const base::Callback<void(const std::list<IndexedDBInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;
  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(
          &CannedBrowsingDataIndexedDBHelper::ConvertPendingInfoInWebKitThread,
          this));
}

void CannedBrowsingDataIndexedDBHelper::ConvertPendingInfoInWebKitThread() {
  base::AutoLock auto_lock(lock_);
  indexed_db_info_.clear();
  for (std::set<PendingIndexedDBInfo>::const_iterator
       info = pending_indexed_db_info_.begin();
       info != pending_indexed_db_info_.end(); ++info) {
    indexed_db_info_.push_back(IndexedDBInfo(
        info->origin,
        0,
        base::Time()));
  }

 BrowserThread::PostTask(
     BrowserThread::UI, FROM_HERE,
     base::Bind(&CannedBrowsingDataIndexedDBHelper::NotifyInUIThread, this));
}

void CannedBrowsingDataIndexedDBHelper::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);

  completion_callback_.Run(indexed_db_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
}
