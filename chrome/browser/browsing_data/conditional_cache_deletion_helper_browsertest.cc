// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/conditional_cache_deletion_helper.h"
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
  return url.GetOrigin() == GURL("https://example.com");
}

}  // namespace

class ConditionalCacheDeletionHelperBrowserTest : public InProcessBrowserTest {
 public:
  // Initialization ------------------------------------------------------------

  void SetUpOnMainThread() override {
    // Prepare the commonly used callbacks.
    done_callback_ = base::Bind(
        &ConditionalCacheDeletionHelperBrowserTest::DoneCallback,
        base::Unretained(this));

    // Get the storage partition.
    partition_ = content::BrowserContext::GetDefaultStoragePartition(
        browser()->profile());

    // Get the cache backends.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ConditionalCacheDeletionHelperBrowserTest::SetUpOnIOThread,
                   base::Unretained(this)));
    WaitForTasksOnIOThread();
  }

  void SetUpOnIOThread() {
    net::URLRequestContextGetter* context = partition_->GetURLRequestContext();

    net::HttpCache* cache = context->GetURLRequestContext()->
        http_transaction_factory()->GetCache();

    SetNumberOfWaitedTasks(1);
    WaitForCompletion(cache->GetBackend(&backend_, done_callback_));
  }

  void TearDownOnMainThread() override {
    // The cache iterator must be deleted on the thread where it was created,
    // which is the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &ConditionalCacheDeletionHelperBrowserTest::TearDownOnIOThread,
            base::Unretained(this)));
    WaitForTasksOnIOThread();
  }

  void TearDownOnIOThread() {
    iterator_.reset();
    DoneCallback(net::OK);
  }

  // Waiting for tasks to be done on IO thread. --------------------------------

  void WaitForTasksOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    io_thread_loop_.reset(new base::RunLoop());
    io_thread_loop_->Run();
  }

  void SetNumberOfWaitedTasks(int count) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    remaining_tasks_ = count;
  }

  void WaitForCompletion(int value) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (value >= 0) {
      // We got the result immediately.
      DoneCallback(value);
    } else if (value == net::ERR_IO_PENDING) {
      // We need to wait for the callback.
    } else {
      // An error has occurred.
      NOTREACHED();
    }
  }

  void DoneCallback(int value) {
    DCHECK_GE(value, 0);  // Negative values represent an error.
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (--remaining_tasks_ > 0)
      return;

    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&base::RunLoop::Quit,
                   base::Unretained(io_thread_loop_.get())));
  }

  // Cache operation shorthands. -----------------------------------------------

  void CreateCacheEntries(const std::set<std::string>& keys) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    entries_.resize(keys.size());
    SetNumberOfWaitedTasks(keys.size());

    int pos = 0;
    for (const std::string& key : keys) {
      WaitForCompletion(backend_->CreateEntry(
          key, &entries_[pos++], done_callback_));
    }
  }

  void DeleteEntries(
      const base::Callback<bool(const disk_cache::Entry*)>& condition) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ConditionalCacheDeletionHelper* helper = new ConditionalCacheDeletionHelper(
        backend_,
        condition);

    WaitForCompletion(helper->DeleteAndDestroySelfWhenFinished(done_callback_));
  }

  void GetRemainingKeys() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    current_entry_ = nullptr;
    iterator_ = backend_->CreateIterator();
    GetNextKey(net::OK);
  }

  void GetNextKey(int error) {
    while (error != net::ERR_IO_PENDING) {
      if (error == net::ERR_FAILED) {
        DoneCallback(net::OK);
        return;
      }

      if (current_entry_) {
        remaining_keys_.push_back(current_entry_->GetKey());
      }

      error = iterator_->OpenNextEntry(
          &current_entry_,
          base::Bind(&ConditionalCacheDeletionHelperBrowserTest::GetNextKey,
                     base::Unretained(this)));
    }
  }

  void CompareRemainingKeys(std::set<std::string> expected_set) {
    std::vector<std::string> expected;
    expected.assign(expected_set.begin(), expected_set.end());
    std::sort(remaining_keys_.begin(), remaining_keys_.end());
    EXPECT_EQ(expected, remaining_keys_);
  }

  // Miscellaneous. ------------------------------------------------------------

 private:
  content::StoragePartition* partition_;
  disk_cache::Backend* backend_ = nullptr;
  std::unique_ptr<disk_cache::Backend::Iterator> iterator_;
  disk_cache::Entry* current_entry_;
  std::vector<disk_cache::Entry*> entries_;

  base::Callback<void(int)> done_callback_;

  std::unique_ptr<base::RunLoop> io_thread_loop_;
  int remaining_tasks_;

  std::vector<std::string> remaining_keys_;
};

// Tests that ConditionalCacheDeletionHelper only deletes those cache entries
// that match the condition.
IN_PROC_BROWSER_TEST_F(ConditionalCacheDeletionHelperBrowserTest, Condition) {
  // Create 5 entries.
  std::set<std::string> keys;
  keys.insert("123");
  keys.insert("47");
  keys.insert("56");
  keys.insert("81");
  keys.insert("42");

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::CreateCacheEntries,
                 base::Unretained(this),
                 base::ConstRef(keys)));
  WaitForTasksOnIOThread();

  // Delete the entries whose keys are even numbers.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::DeleteEntries,
                 base::Unretained(this),
                 base::Bind(&KeyIsEven)));
  WaitForTasksOnIOThread();

  // Expect that the keys with values 56 and 42 were deleted.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::GetRemainingKeys,
                 base::Unretained(this)));
  WaitForTasksOnIOThread();

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
IN_PROC_BROWSER_TEST_F(ConditionalCacheDeletionHelperBrowserTest, TimeAndURL) {
  const int64_t timeout_ms = 1;

  // Create some entries.
  std::set<std::string> keys;
  keys.insert("https://google.com/index.html");
  keys.insert("https://example.com/foo/bar/icon.png");
  keys.insert("http://chrome.com");

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::CreateCacheEntries,
                 base::Unretained(this),
                 base::ConstRef(keys)));
  WaitForTasksOnIOThread();

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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::CreateCacheEntries,
                 base::Unretained(this),
                 base::ConstRef(newer_keys)));
  WaitForTasksOnIOThread();

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
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheDeletionHelperBrowserTest::GetRemainingKeys,
                 base::Unretained(this)));
  WaitForTasksOnIOThread();

  keys.insert(newer_keys.begin(), newer_keys.end());
  keys.erase("https://example.com/foo/bar/icon2.png");
  keys.erase("https://example.com/foo/bar/icon3.png");
  CompareRemainingKeys(keys);
}
