// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/browsing_data/cache_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/content/conditional_cache_deletion_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using browsing_data::ConditionalCacheDeletionHelper;
using content::BrowserThread;

namespace {

bool KeyIsEven(const disk_cache::Entry* entry) {
  int key_as_int = 0;
  base::StringToInt(entry->GetKey().c_str(), &key_as_int);
  return (key_as_int % 2) == 0;
}

bool HasHttpsExampleOrigin(const GURL& url) {
  return url.GetOrigin() == "https://example.com/";
}

}  // namespace

class ConditionalCacheDeletionHelperBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    cache_util_ = base::MakeUnique<CacheTestUtil>(
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile()));
    done_callback_ =
        base::Bind(&ConditionalCacheDeletionHelperBrowserTest::DoneCallback,
                   base::Unretained(this));
    // UI and IO thread synchronization.
    waitable_event_ = base::MakeUnique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  }

  void TearDownOnMainThread() override { cache_util_.reset(); }

  void DeleteEntries(
      const base::Callback<bool(const disk_cache::Entry*)>& condition) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    auto* helper =
        new ConditionalCacheDeletionHelper(cache_util_->backend(), condition);

    helper->DeleteAndDestroySelfWhenFinished(done_callback_);
  }

  void CompareRemainingKeys(std::set<std::string> expected_set) {
    std::vector<std::string> remaining_keys = cache_util_->GetEntryKeys();
    std::sort(remaining_keys.begin(), remaining_keys.end());
    std::vector<std::string> expected;
    expected.assign(expected_set.begin(), expected_set.end());
    EXPECT_EQ(expected, remaining_keys);
  }

  void DoneCallback(int value) {
    DCHECK_GE(value, 0);  // Negative values represent an error.
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    waitable_event_->Signal();
  }

  void WaitForTasksOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    waitable_event_->Wait();
  }

  CacheTestUtil* GetCacheTestUtil() { return cache_util_.get(); }

 private:
  base::Callback<void(int)> done_callback_;
  std::unique_ptr<CacheTestUtil> cache_util_;
  std::unique_ptr<base::WaitableEvent> waitable_event_;
};

// Tests that ConditionalCacheDeletionHelper only deletes those cache entries
// that match the condition.
IN_PROC_BROWSER_TEST_F(ConditionalCacheDeletionHelperBrowserTest, Condition) {
  // Create 5 entries.
  std::set<std::string> keys = {"123", "47", "56", "81", "42"};

  GetCacheTestUtil()->CreateCacheEntries(keys);

  // Delete the entries whose keys are even numbers.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::DeleteEntries,
                 base::Unretained(this),
                 base::Bind(&KeyIsEven)));
  WaitForTasksOnIOThread();

  // Expect that the keys with values 56 and 42 were deleted.
  keys.erase("56");
  keys.erase("42");
  CompareRemainingKeys(keys);
}

// Tests that ConditionalCacheDeletionHelper correctly constructs a condition
// for time and URL.
//
// Note: This test depends on the timing in cache backends and can be flaky
// if those backends are slow. If this turns out to be a problem, consider
// increasing the |timeout_ms| constant.
//
// Flakily timing out on Mac 10.11 (crbug.com/646119) and flakily
// failing on Linux/ChromeOS (crbug.com/624836).
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_TimeAndURL DISABLED_TimeAndURL
#else
#define MAYBE_TimeAndURL TimeAndURL
#endif
IN_PROC_BROWSER_TEST_F(ConditionalCacheDeletionHelperBrowserTest,
                       MAYBE_TimeAndURL) {
  const int64_t timeout_ms = 1;

  // Create some entries.
  std::set<std::string> keys;
  keys.insert("https://google.com/index.html");
  keys.insert("https://example.com/foo/bar/icon.png");
  keys.insert("http://chrome.com");

  GetCacheTestUtil()->CreateCacheEntries(keys);

  // Wait |timeout_ms| milliseconds for the cache to write the entries.
  // This assures that future entries will have timestamps strictly greater than
  // the ones we just added.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(timeout_ms));
  base::Time now = base::Time::Now();

  // Create a few more entries with a later timestamp.
  std::set<std::string> newer_keys;
  newer_keys.insert("https://google.com/");
  newer_keys.insert("https://example.com/foo/bar/icon2.png");
  newer_keys.insert("https://example.com/foo/bar/icon3.png");
  newer_keys.insert("http://example.com/foo/bar/icon4.png");

  GetCacheTestUtil()->CreateCacheEntries(newer_keys);

  // Create a condition for entries with the "https://example.com" origin
  // created after waiting.
  base::Callback<bool(const disk_cache::Entry*)> condition =
      ConditionalCacheDeletionHelper::CreateURLAndTimeCondition(
          base::Bind(&HasHttpsExampleOrigin), now, base::Time::Max());

  // Delete the entries.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::DeleteEntries,
                 base::Unretained(this),
                 base::ConstRef(condition)));
  WaitForTasksOnIOThread();

  // Expect that only "icon2.png" and "icon3.png" were deleted.
  keys.insert(newer_keys.begin(), newer_keys.end());
  keys.erase("https://example.com/foo/bar/icon2.png");
  keys.erase("https://example.com/foo/bar/icon3.png");
  CompareRemainingKeys(keys);
}
