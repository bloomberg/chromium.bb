// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

namespace web {
class BrowserState;
}

class CacheCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit CacheCounter(web::BrowserState* browser_state);
  ~CacheCounter() override;

  // BrowsingDataCounter implementation.
  const char* GetPrefName() const override;

 private:
  // BrowsingDataCounter implementation.
  void Count() override;

  void OnCacheSizeCalculated(int result_bytes);

  web::BrowserState* browser_state_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
