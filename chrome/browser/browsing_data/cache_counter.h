// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"

class CacheCounter: public BrowsingDataCounter {
 public:
  CacheCounter();
  ~CacheCounter() override;

  const std::string& GetPrefName() const override;

  // Whether this counter awaits the calculation result callback.
  // Used only for testing.
  bool Pending();

 private:
  const std::string pref_name_;
  bool pending_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_;

  void Count() override;
  void OnCacheSizeCalculated(int64 bytes);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CACHE_COUNTER_H_
