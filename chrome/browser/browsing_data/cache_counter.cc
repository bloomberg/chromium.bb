// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cache_counter.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/storage_partition_http_cache_data_remover.h"
#include "net/base/net_errors.h"

CacheCounter::CacheCounter() : pref_name_(prefs::kDeleteCache),
                               pending_(false),
                               weak_ptr_factory_(this) {
}

CacheCounter::~CacheCounter() {
}

const std::string& CacheCounter::GetPrefName() const {
  return pref_name_;
}

void CacheCounter::Count() {
  // TODO(msramek): StoragePartitionHttpCacheDataRemover currently does not
  // implement counting for subsets of cache, only for the entire cache. Thus,
  // we ignore the time period setting and always request counting for
  // the unbounded time interval. It is up to the UI to interpret the results
  // for finite time intervals as upper estimates.
  browsing_data::StoragePartitionHttpCacheDataRemover::CreateForRange(
      content::BrowserContext::GetDefaultStoragePartition(GetProfile()),
      base::Time(),
      base::Time::Max())->Count(
          base::Bind(&CacheCounter::OnCacheSizeCalculated,
                     weak_ptr_factory_.GetWeakPtr()));
  pending_ = true;
}

void CacheCounter::OnCacheSizeCalculated(int64 result_bytes) {
  pending_ = false;

  // A value less than 0 means a net error code.
  if (result_bytes < 0)
    return;

  ReportResult(result_bytes);
}

bool CacheCounter::Pending() {
  return pending_;
}
