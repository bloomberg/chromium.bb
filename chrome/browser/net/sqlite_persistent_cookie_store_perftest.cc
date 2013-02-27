// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/perftimer.h"
#include "base/sequenced_task_runner.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

class SQLitePersistentCookieStorePerfTest : public testing::Test {
 public:
  SQLitePersistentCookieStorePerfTest()
      : pool_owner_(new base::SequencedWorkerPoolOwner(1, "Background Pool")),
        loaded_event_(false, false),
        key_loaded_event_(false, false) {
  }

  void OnLoaded(const std::vector<net::CanonicalCookie*>& cookies) {
    cookies_ = cookies;
    loaded_event_.Signal();
  }

  void OnKeyLoaded(const std::vector<net::CanonicalCookie*>& cookies) {
    cookies_ = cookies;
    key_loaded_event_.Signal();
  }

  void Load() {
    store_->Load(base::Bind(&SQLitePersistentCookieStorePerfTest::OnLoaded,
                                base::Unretained(this)));
    loaded_event_.Wait();
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("background"));
  }

  scoped_refptr<base::SequencedTaskRunner> client_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("client"));
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLitePersistentCookieStore(
        temp_dir_.path().Append(chrome::kCookieFilename),
        client_task_runner(),
        background_task_runner(),
        false, NULL);
    std::vector<net::CanonicalCookie*> cookies;
    Load();
    ASSERT_EQ(0u, cookies_.size());
    // Creates 15000 cookies from 300 eTLD+1s.
    base::Time t = base::Time::Now();
    for (int domain_num = 0; domain_num < 300; domain_num++) {
      std::string domain_name(base::StringPrintf(".domain_%d.com", domain_num));
      GURL gurl("www" + domain_name);
      for (int cookie_num = 0; cookie_num < 50; ++cookie_num) {
        t += base::TimeDelta::FromInternalValue(10);
        store_->AddCookie(
            net::CanonicalCookie(gurl,
                base::StringPrintf("Cookie_%d", cookie_num), "1",
                domain_name, "/", std::string(), std::string(),
                t, t, t, false, false));
      }
    }
    // Replace the store effectively destroying the current one and forcing it
    // to write its data to disk.
    store_ = NULL;

    // Shut down the pool, causing deferred (no-op) commits to be discarded.
    pool_owner_->pool()->Shutdown();
    // ~SequencedWorkerPoolOwner blocks on pool shutdown.
    pool_owner_.reset(new base::SequencedWorkerPoolOwner(1, "pool"));

    store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename),
      client_task_runner(),
      background_task_runner(),
      false, NULL);
  }

  virtual void TearDown() OVERRIDE {
    store_ = NULL;
    pool_owner_->pool()->Shutdown();
  }

 protected:
  scoped_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  base::WaitableEvent loaded_event_;
  base::WaitableEvent key_loaded_event_;
  std::vector<net::CanonicalCookie*> cookies_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
};

// Test the performance of priority load of cookies for a specfic domain key
TEST_F(SQLitePersistentCookieStorePerfTest, TestLoadForKeyPerformance) {
  for (int domain_num = 0; domain_num < 3; ++domain_num) {
    std::string domain_name(base::StringPrintf("domain_%d.com", domain_num));
    PerfTimeLogger timer(
      ("Load cookies for the eTLD+1 " + domain_name).c_str());
    store_->LoadCookiesForKey(domain_name,
      base::Bind(&SQLitePersistentCookieStorePerfTest::OnKeyLoaded,
                 base::Unretained(this)));
    key_loaded_event_.Wait();
    timer.Done();

    ASSERT_EQ(50U, cookies_.size());
  }
}

// Test the performance of load
TEST_F(SQLitePersistentCookieStorePerfTest, TestLoadPerformance) {
  PerfTimeLogger timer("Load all cookies");
  Load();
  timer.Done();

  ASSERT_EQ(15000U, cookies_.size());
}
