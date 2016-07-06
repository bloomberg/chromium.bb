// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/counters/browsing_data_counter.h"

class Profile;

class CacheCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit CacheCounter(Profile* profile);
  ~CacheCounter() override;

  // Whether this counter awaits the calculation result callback.
  // Used only for testing.
  bool Pending();

 private:
  Profile* profile_;
  bool pending_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_;

  void Count() override;
  void OnCacheSizeCalculated(int64_t bytes);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
