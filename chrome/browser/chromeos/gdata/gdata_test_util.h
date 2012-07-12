// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_

#include "chrome/browser/chromeos/gdata/gdata_cache_entry.h"

namespace gdata {
namespace test_util {

// Runs a task posted to the blocking pool, including subquent tasks posted
// to the UI message loop and the blocking pool.
//
// A task is often posted to the blocking pool with PostTaskAndReply(). In
// that case, a task is posted back to the UI message loop, which can again
// post a task to the blocking pool. This function processes these tasks
// repeatedly.
void RunBlockingPoolTask();

// Converts |cache_state| to a GDataCacheEntry.
GDataCacheEntry ToCacheEntry(int cache_state);

// Returns true if the cache state of the given two cache entries are equal.
bool CacheStatesEqual(const GDataCacheEntry& a, const GDataCacheEntry& b);

}  // namespace test_util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_
