// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_database_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "storage/common/database/database_identifier.h"

using content::BrowserContext;
using content::BrowserThread;
using content::StorageUsageInfo;
using storage::DatabaseIdentifier;

BrowsingDataDatabaseHelper::BrowsingDataDatabaseHelper(Profile* profile)
    : tracker_(BrowserContext::GetDefaultStoragePartition(profile)
                   ->GetDatabaseTracker()) {}

BrowsingDataDatabaseHelper::~BrowsingDataDatabaseHelper() {
}

void BrowsingDataDatabaseHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskAndReplyWithResult(
      tracker_->task_runner(), FROM_HERE,
      base::BindOnce(
          [](storage::DatabaseTracker* tracker) {
            std::list<StorageUsageInfo> result;
            std::vector<storage::OriginInfo> origins_info;
            if (tracker->GetAllOriginsInfo(&origins_info)) {
              for (const storage::OriginInfo& info : origins_info) {
                url::Origin origin = storage::GetOriginFromIdentifier(
                    info.GetOriginIdentifier());
                if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
                  continue;
                result.emplace_back(origin, info.TotalSize(),
                                    info.LastModified());
              }
            }
            return result;
          },
          base::RetainedRef(tracker_)),
      std::move(callback));
}

void BrowsingDataDatabaseHelper::DeleteDatabase(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  tracker_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::DatabaseTracker::DeleteDataForOrigin),
          tracker_, origin, net::CompletionCallback()));
}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::PendingDatabaseInfo(
    const GURL& origin)
    : origin(origin) {}

CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::~PendingDatabaseInfo() {}

bool CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo::operator<(
    const PendingDatabaseInfo& other) const {
  return origin < other.origin;
}

CannedBrowsingDataDatabaseHelper::CannedBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile) {
}

void CannedBrowsingDataDatabaseHelper::AddDatabase(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.
  pending_database_info_.insert(PendingDatabaseInfo(origin));
}

void CannedBrowsingDataDatabaseHelper::Reset() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pending_database_info_.clear();
}

bool CannedBrowsingDataDatabaseHelper::empty() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_database_info_.empty();
}

size_t CannedBrowsingDataDatabaseHelper::GetDatabaseCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_database_info_.size();
}

const std::set<CannedBrowsingDataDatabaseHelper::PendingDatabaseInfo>&
CannedBrowsingDataDatabaseHelper::GetPendingDatabaseInfo() {
  return pending_database_info_;
}

void CannedBrowsingDataDatabaseHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const PendingDatabaseInfo& info : pending_database_info_) {
    result.push_back(content::StorageUsageInfo(url::Origin::Create(info.origin),
                                               0, base::Time()));
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataDatabaseHelper::DeleteDatabase(
    const url::Origin& origin) {
  GURL origin_url = origin.GetURL();
  base::EraseIf(pending_database_info_,
                [&origin_url](const PendingDatabaseInfo& info) {
                  return info.origin == origin_url;
                });
  BrowsingDataDatabaseHelper::DeleteDatabase(origin);
}

CannedBrowsingDataDatabaseHelper::~CannedBrowsingDataDatabaseHelper() {}
