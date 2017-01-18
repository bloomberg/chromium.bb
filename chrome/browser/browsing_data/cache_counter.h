// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class Profile;

namespace browsing_data {
class ConditionalCacheCountingHelper;
}

class CacheCounter : public browsing_data::BrowsingDataCounter {
 public:
  class CacheResult : public FinishedResult {
   public:
    CacheResult(const CacheCounter* source,
                int64_t cache_size,
                bool is_upper_limit);
    ~CacheResult() override;

    int64_t cache_size() const { return cache_size_; }
    bool is_upper_limit() const { return is_upper_limit_; }

   private:
    int64_t cache_size_;
    bool is_upper_limit_;

    DISALLOW_COPY_AND_ASSIGN(CacheResult);
  };

  explicit CacheCounter(Profile* profile);
  ~CacheCounter() override;

  const char* GetPrefName() const override;

 private:
  void Count() override;
  void OnCacheSizeCalculated(int64_t bytes, bool is_upper_limit);
  void FetchEstimate(
      base::WeakPtr<browsing_data::ConditionalCacheCountingHelper>);

  Profile* profile_;
  bool pending_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_;

};

#endif  // CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
