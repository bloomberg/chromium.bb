// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace {

// Only websafe state and suborigins are considered browsing data.
bool HasStorageScheme(const GURL& origin) {
  return BrowsingDataHelper::HasWebScheme(origin) ||
         origin.scheme() == content::kHttpSuboriginScheme ||
         origin.scheme() == content::kHttpsSuboriginScheme;
}

void GetUsageInfoCallback(
    const BrowsingDataLocalStorageHelper::FetchCallback& callback,
    const std::vector<content::LocalStorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo> result;
  for (const content::LocalStorageUsageInfo& info : infos) {
    if (!HasStorageScheme(info.origin))
      continue;
    result.push_back(BrowsingDataLocalStorageHelper::LocalStorageInfo(
        info.origin, info.data_size, info.last_modified));
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, result));
}

}  // namespace

BrowsingDataLocalStorageHelper::LocalStorageInfo::LocalStorageInfo(
    const GURL& origin_url,
    int64_t size,
    base::Time last_modified)
    : origin_url(origin_url), size(size), last_modified(last_modified) {}

BrowsingDataLocalStorageHelper::LocalStorageInfo::~LocalStorageInfo() {}

BrowsingDataLocalStorageHelper::BrowsingDataLocalStorageHelper(Profile* profile)
    : dom_storage_context_(BrowserContext::GetDefaultStoragePartition(profile)
                               ->GetDOMStorageContext()) {
  DCHECK(dom_storage_context_);
}

BrowsingDataLocalStorageHelper::~BrowsingDataLocalStorageHelper() {
}

void BrowsingDataLocalStorageHelper::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  dom_storage_context_->GetLocalStorageUsage(
      base::Bind(&GetUsageInfoCallback, callback));
}

void BrowsingDataLocalStorageHelper::DeleteOrigin(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dom_storage_context_->DeleteLocalStorage(origin);
}

//---------------------------------------------------------

CannedBrowsingDataLocalStorageHelper::CannedBrowsingDataLocalStorageHelper(
    Profile* profile)
    : BrowsingDataLocalStorageHelper(profile) {
}

void CannedBrowsingDataLocalStorageHelper::AddLocalStorage(
    const GURL& origin) {
  if (!HasStorageScheme(origin))
    return;
  pending_local_storage_info_.insert(origin);
  if (content::HasSuborigin(origin)) {
    pending_origins_to_pending_suborigins_.insert(
        std::make_pair(content::StripSuboriginFromUrl(origin), origin));
  }
}

void CannedBrowsingDataLocalStorageHelper::Reset() {
  pending_local_storage_info_.clear();
  pending_origins_to_pending_suborigins_.clear();
}

bool CannedBrowsingDataLocalStorageHelper::empty() const {
  return pending_local_storage_info_.empty();
}

size_t CannedBrowsingDataLocalStorageHelper::GetLocalStorageCount() const {
  return pending_local_storage_info_.size();
}

const std::set<GURL>&
CannedBrowsingDataLocalStorageHelper::GetLocalStorageInfo() const {
  return pending_local_storage_info_;
}

void CannedBrowsingDataLocalStorageHelper::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<LocalStorageInfo> result;
  for (const GURL& url : pending_local_storage_info_)
    result.push_back(LocalStorageInfo(url, 0, base::Time()));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(callback, result));
}

void CannedBrowsingDataLocalStorageHelper::DeleteOrigin(const GURL& origin) {
  pending_local_storage_info_.erase(origin);
  // All suborigins associated with |origin| must be removed.
  // BrowsingDataLocalStorageHelper::DeleteOrigin takes care of doing that on
  // the backend so it's not necessary to call it for each suborigin, but it is
  // necessary to clear up the pending storage here.
  BrowsingDataLocalStorageHelper::DeleteOrigin(origin);
  if (pending_origins_to_pending_suborigins_.count(origin) > 0) {
    auto it = pending_origins_to_pending_suborigins_.find(origin);
    while (it != pending_origins_to_pending_suborigins_.end()) {
      pending_local_storage_info_.erase(it->second);
      pending_origins_to_pending_suborigins_.erase(it);
      it = pending_origins_to_pending_suborigins_.find(origin);
    }
  }
  pending_origins_to_pending_suborigins_.erase(origin);
}

CannedBrowsingDataLocalStorageHelper::~CannedBrowsingDataLocalStorageHelper() {}
