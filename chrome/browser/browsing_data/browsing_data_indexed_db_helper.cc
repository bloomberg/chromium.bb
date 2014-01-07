// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"

using content::BrowserThread;
using content::IndexedDBContext;
using content::IndexedDBInfo;

BrowsingDataIndexedDBHelper::BrowsingDataIndexedDBHelper(
    IndexedDBContext* indexed_db_context)
    : indexed_db_context_(indexed_db_context),
      is_fetching_(false) {
  DCHECK(indexed_db_context_.get());
}

BrowsingDataIndexedDBHelper::~BrowsingDataIndexedDBHelper() {
}

void BrowsingDataIndexedDBHelper::StartFetching(
    const base::Callback<void(const std::list<IndexedDBInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BrowsingDataIndexedDBHelper::FetchIndexedDBInfoInIndexedDBThread,
          this));
}

void BrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BrowsingDataIndexedDBHelper::DeleteIndexedDBInIndexedDBThread,
          this,
          origin));
}

void BrowsingDataIndexedDBHelper::FetchIndexedDBInfoInIndexedDBThread() {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<IndexedDBInfo> origins = indexed_db_context_->GetAllOriginsInfo();
  for (std::vector<IndexedDBInfo>::const_iterator iter = origins.begin();
       iter != origins.end(); ++iter) {
    const IndexedDBInfo& origin = *iter;
    if (!BrowsingDataHelper::HasWebScheme(origin.origin_))
      continue;  // Non-websafe state is not considered browsing data.

    indexed_db_info_.push_back(origin);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataIndexedDBHelper::NotifyInUIThread, this));
}

void BrowsingDataIndexedDBHelper::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  completion_callback_.Run(indexed_db_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
}

void BrowsingDataIndexedDBHelper::DeleteIndexedDBInIndexedDBThread(
    const GURL& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  indexed_db_context_->DeleteForOrigin(origin);
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::PendingIndexedDBInfo(const GURL& origin,
                                           const base::string16& name)
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

CannedBrowsingDataIndexedDBHelper::CannedBrowsingDataIndexedDBHelper(
    content::IndexedDBContext* context)
    : BrowsingDataIndexedDBHelper(context) {
}

CannedBrowsingDataIndexedDBHelper::~CannedBrowsingDataIndexedDBHelper() {}

CannedBrowsingDataIndexedDBHelper* CannedBrowsingDataIndexedDBHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataIndexedDBHelper* clone =
      new CannedBrowsingDataIndexedDBHelper(indexed_db_context_);

  clone->pending_indexed_db_info_ = pending_indexed_db_info_;
  clone->indexed_db_info_ = indexed_db_info_;
  return clone;
}

void CannedBrowsingDataIndexedDBHelper::AddIndexedDB(
    const GURL& origin, const base::string16& name) {
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  pending_indexed_db_info_.insert(PendingIndexedDBInfo(origin, name));
}

void CannedBrowsingDataIndexedDBHelper::Reset() {
  indexed_db_info_.clear();
  pending_indexed_db_info_.clear();
}

bool CannedBrowsingDataIndexedDBHelper::empty() const {
  return indexed_db_info_.empty() && pending_indexed_db_info_.empty();
}

size_t CannedBrowsingDataIndexedDBHelper::GetIndexedDBCount() const {
  return pending_indexed_db_info_.size();
}

const std::set<CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo>&
CannedBrowsingDataIndexedDBHelper::GetIndexedDBInfo() const  {
  return pending_indexed_db_info_;
}

void CannedBrowsingDataIndexedDBHelper::StartFetching(
    const base::Callback<void(const std::list<IndexedDBInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::list<IndexedDBInfo> result;
  for (std::set<PendingIndexedDBInfo>::const_iterator
       pending_info = pending_indexed_db_info_.begin();
       pending_info != pending_indexed_db_info_.end(); ++pending_info) {
    IndexedDBInfo info(
        pending_info->origin, 0, base::Time(), base::FilePath(), 0);
    result.push_back(info);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
}

void CannedBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const GURL& origin) {
  for (std::set<PendingIndexedDBInfo>::iterator it =
           pending_indexed_db_info_.begin();
       it != pending_indexed_db_info_.end(); ) {
    if (it->origin == origin)
      pending_indexed_db_info_.erase(it++);
    else
      ++it;
  }
  BrowsingDataIndexedDBHelper::DeleteIndexedDB(origin);
}
