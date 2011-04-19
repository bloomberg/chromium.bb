// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/thread_test_helper.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class SQLitePersistentCookieStoreTest : public testing::Test {
 public:
  SQLitePersistentCookieStoreTest()
      : ui_thread_(BrowserThread::UI),
        db_thread_(BrowserThread::DB) {
  }

 protected:
  virtual void SetUp() {
    ui_thread_.Start();
    db_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLitePersistentCookieStore(
        temp_dir_.path().Append(chrome::kCookieFilename));
    std::vector<net::CookieMonster::CanonicalCookie*> cookies;
    ASSERT_TRUE(store_->Load(&cookies));
    ASSERT_TRUE(0 == cookies.size());
    // Make sure the store gets written at least once.
    store_->AddCookie(
        net::CookieMonster::CanonicalCookie(GURL(), "A", "B", "http://foo.bar",
                                            "/", base::Time::Now(),
                                            base::Time::Now(),
                                            base::Time::Now(),
                                            false, false, true));
  }

  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
};

TEST_F(SQLitePersistentCookieStoreTest, KeepOnDestruction) {
  store_->SetClearLocalStateOnExit(false);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  scoped_refptr<ThreadTestHelper> helper(
      new ThreadTestHelper(BrowserThread::DB));
  ASSERT_TRUE(helper->Run());

  ASSERT_TRUE(file_util::PathExists(
      temp_dir_.path().Append(chrome::kCookieFilename)));
  ASSERT_TRUE(file_util::Delete(
      temp_dir_.path().Append(chrome::kCookieFilename), false));
}

TEST_F(SQLitePersistentCookieStoreTest, RemoveOnDestruction) {
  store_->SetClearLocalStateOnExit(true);
  // Replace the store effectively destroying the current one and forcing it
  // to write it's data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  scoped_refptr<ThreadTestHelper> helper(
      new ThreadTestHelper(BrowserThread::DB));
  ASSERT_TRUE(helper->Run());

  ASSERT_FALSE(file_util::PathExists(
      temp_dir_.path().Append(chrome::kCookieFilename)));
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLitePersistentCookieStoreTest, TestPersistance) {
  std::vector<net::CookieMonster::CanonicalCookie*> cookies;
  // Replace the store effectively destroying the current one and forcing it
  // to write it's data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  scoped_refptr<ThreadTestHelper> helper(
      new ThreadTestHelper(BrowserThread::DB));
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename));

  // Reload and test for persistence
  ASSERT_TRUE(store_->Load(&cookies));
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("http://foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());

  // Now delete the cookie and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  STLDeleteContainerPointers(cookies.begin(), cookies.end());
  cookies.clear();
  store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename));

  // Reload and check if the cookie has been removed.
  ASSERT_TRUE(store_->Load(&cookies));
  ASSERT_EQ(0U, cookies.size());
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlush) {
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  FilePath path = temp_dir_.path().Append(chrome::kCookieFilename);
  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  int64 base_size = info.size;

  // Write some large cookies, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::TimeDelta::FromMicroseconds(c);
    std::string name(1, c);
    std::string value(1000, c);
    store_->AddCookie(
        net::CookieMonster::CanonicalCookie(GURL(), name, value,
                                            "http://foo.bar", "/", t, t, t,
                                            false, false, true));
  }

  // Call Flush() and wait until the DB thread is idle.
  store_->Flush(NULL);
  scoped_refptr<ThreadTestHelper> helper(
      new ThreadTestHelper(BrowserThread::DB));
  ASSERT_TRUE(helper->Run());

  // We forced a write, so now the file will be bigger.
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  ASSERT_GT(info.size, base_size);
}

// Counts the number of times Callback() has been run.
class CallbackCounter : public base::RefCountedThreadSafe<CallbackCounter> {
 public:
  CallbackCounter() : callback_count_(0) {}

  void Callback() {
    ++callback_count_;
  }

  int callback_count() {
    return callback_count_;
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackCounter>;
  volatile int callback_count_;
};

// Test that we can get a completion callback after a Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlushCompletionCallback) {
  scoped_refptr<CallbackCounter> counter(new CallbackCounter());

  // Callback shouldn't be invoked until we call Flush().
  ASSERT_EQ(0, counter->callback_count());

  store_->Flush(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));

  scoped_refptr<ThreadTestHelper> helper(
      new ThreadTestHelper(BrowserThread::DB));
  ASSERT_TRUE(helper->Run());

  ASSERT_EQ(1, counter->callback_count());
}
