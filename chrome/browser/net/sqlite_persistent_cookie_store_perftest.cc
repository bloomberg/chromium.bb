// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/perftimer.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class SQLitePersistentCookieStorePerfTest : public testing::Test {
 public:
  SQLitePersistentCookieStorePerfTest()
      : db_thread_(BrowserThread::DB),
        io_thread_(BrowserThread::IO),
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

  virtual void SetUp() {
    db_thread_.Start();
    io_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLitePersistentCookieStore(
        temp_dir_.path().Append(chrome::kCookieFilename),
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
    scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
    // Make sure we wait until the destructor has run.
    ASSERT_TRUE(helper->Run());

    store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename), false, NULL);
  }

 protected:
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread io_thread_;
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
