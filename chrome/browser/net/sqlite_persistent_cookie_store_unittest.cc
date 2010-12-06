// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/thread_test_helper.h"
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
        net::CookieMonster::CanonicalCookie("A", "B", "http://foo.bar", "/",
                                            false, false,
                                            base::Time::Now(),
                                            base::Time::Now(),
                                            true, base::Time::Now()));
  }

  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
  ScopedTempDir temp_dir_;
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
