// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cache_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "components/browsing_data/core/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

CacheCounter::CacheResult::CacheResult(const CacheCounter* source,
                                       int64_t cache_size,
                                       bool is_upper_limit)
    : FinishedResult(source, cache_size),
      cache_size_(cache_size),
      is_upper_limit_(is_upper_limit) {}

CacheCounter::CacheResult::~CacheResult() {}

CacheCounter::CacheCounter(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {}

CacheCounter::~CacheCounter() {
}

const char* CacheCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteCache;
}

void CacheCounter::Count() {
  base::WeakPtr<browsing_data::ConditionalCacheCountingHelper> counter =
      browsing_data::ConditionalCacheCountingHelper::CreateForRange(
          content::BrowserContext::GetDefaultStoragePartition(profile_),
          GetPeriodStart(), base::Time::Max())
          ->CountAndDestroySelfWhenFinished(
              base::Bind(&CacheCounter::OnCacheSizeCalculated,
                         weak_ptr_factory_.GetWeakPtr()));
}

void CacheCounter::OnCacheSizeCalculated(int64_t result_bytes,
                                         bool is_upper_limit) {
  // A value less than 0 means a net error code.
  if (result_bytes < 0)
    return;
  auto result =
      base::MakeUnique<CacheResult>(this, result_bytes, is_upper_limit);
  ReportResult(std::move(result));
}
