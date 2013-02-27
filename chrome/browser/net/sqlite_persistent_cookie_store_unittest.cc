// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "chrome/browser/net/clear_on_exit_policy.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/mock_special_storage_policy.h"

typedef std::vector<net::CanonicalCookie*> CanonicalCookieVector;

class SQLitePersistentCookieStoreTest : public testing::Test {
 public:
  SQLitePersistentCookieStoreTest()
      : pool_owner_(new base::SequencedWorkerPoolOwner(3, "Background Pool")),
        loaded_event_(false, false),
        key_loaded_event_(false, false),
        db_thread_event_(false, false) {
  }

  void OnLoaded(const CanonicalCookieVector& cookies) {
    cookies_ = cookies;
    loaded_event_.Signal();
  }

  void OnKeyLoaded(const CanonicalCookieVector& cookies) {
    cookies_ = cookies;
    key_loaded_event_.Signal();
  }

  void Load(CanonicalCookieVector* cookies) {
    EXPECT_FALSE(loaded_event_.IsSignaled());
    store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                            base::Unretained(this)));
    loaded_event_.Wait();
    *cookies = cookies_;
  }

  void Flush() {
    base::WaitableEvent event(false, false);
    store_->Flush(base::Bind(&base::WaitableEvent::Signal,
                             base::Unretained(&event)));
    event.Wait();
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("background"));
  }

  scoped_refptr<base::SequencedTaskRunner> client_task_runner() {
    return pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("client"));
  }

  void DestroyStore() {
    store_ = NULL;
    // Make sure we wait until the destructor has run by shutting down the pool
    // resetting the owner (whose destructor blocks on the pool completion).
    pool_owner_->pool()->Shutdown();
    // Create a new pool for the few tests that create multiple stores. In other
    // cases this is wasted but harmless.
    pool_owner_.reset(new base::SequencedWorkerPoolOwner(3, "Background Pool"));
  }

  void CreateAndLoad(bool restore_old_session_cookies,
                     CanonicalCookieVector* cookies) {
    store_ = new SQLitePersistentCookieStore(
        temp_dir_.path().Append(chrome::kCookieFilename),
        client_task_runner(),
        background_task_runner(),
        restore_old_session_cookies,
        NULL);
    Load(cookies);
  }

  void InitializeStore(bool restore_old_session_cookies) {
    CanonicalCookieVector cookies;
    CreateAndLoad(restore_old_session_cookies, &cookies);
    EXPECT_EQ(0U, cookies.size());
  }

  // We have to create this method to wrap WaitableEvent::Wait, since we cannot
  // bind a non-void returning method as a Closure.
  void WaitOnDBEvent() {
    db_thread_event_.Wait();
  }

  // Adds a persistent cookie to store_.
  void AddCookie(const std::string& name,
                 const std::string& value,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& creation) {
    store_->AddCookie(
        net::CanonicalCookie(GURL(), name, value, domain, path, std::string(),
                             std::string(), creation, creation, creation, false,
                             false));
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    DestroyStore();
    pool_owner_->pool()->Shutdown();
  }

 protected:
  MessageLoop main_loop_;
  scoped_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  base::WaitableEvent loaded_event_;
  base::WaitableEvent key_loaded_event_;
  base::WaitableEvent db_thread_event_;
  CanonicalCookieVector cookies_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
};

TEST_F(SQLitePersistentCookieStoreTest, TestInvalidMetaTableRecovery) {
  InitializeStore(false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  DestroyStore();

  // Load up the store and verify that it has good data in it.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());
  DestroyStore();
  STLDeleteElements(&cookies);

  // Now corrupt the meta table.
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(temp_dir_.path().Append(chrome::kCookieFilename)));
    sql::MetaTable meta_table_;
    meta_table_.Init(&db, 1, 1);
    ASSERT_TRUE(db.Execute("DELETE FROM meta"));
    db.Close();
  }

  // Upon loading, the database should be reset to a good, blank state.
  CreateAndLoad(false, &cookies);
  ASSERT_EQ(0U, cookies.size());

  // Verify that, after, recovery, the database persists properly.
  AddCookie("X", "Y", "foo.bar", "/", base::Time::Now());
  DestroyStore();
  CreateAndLoad(false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("X", cookies[0]->Name().c_str());
  ASSERT_STREQ("Y", cookies[0]->Value().c_str());
  STLDeleteElements(&cookies);
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLitePersistentCookieStoreTest, TestPersistance) {
  InitializeStore(false);
  AddCookie("A", "B", "foo.bar", "/", base::Time::Now());
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  DestroyStore();
  // Reload and test for persistence
  CanonicalCookieVector cookies;
  CreateAndLoad(false, &cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());

  // Now delete the cookie and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  DestroyStore();
  STLDeleteElements(&cookies);

  // Reload and check if the cookie has been removed.
  CreateAndLoad(false, &cookies);
  ASSERT_EQ(0U, cookies.size());
}

// Test that priority load of cookies for a specfic domain key could be
// completed before the entire store is loaded
TEST_F(SQLitePersistentCookieStoreTest, TestLoadCookiesForKey) {
  InitializeStore(false);
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.bar", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "www.aaa.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "travel.aaa.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "www.bbb.com", "/", t);
  DestroyStore();

  store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename),
      client_task_runner(),
      background_task_runner(),
      false, NULL);
  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and LoadCookiesForKey have been posted to its task queue.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                 base::Unretained(this)));
  store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                          base::Unretained(this)));
  store_->LoadCookiesForKey("aaa.com",
    base::Bind(&SQLitePersistentCookieStoreTest::OnKeyLoaded,
               base::Unretained(this)));
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
                 base::Unretained(this)));

  // Now the DB-thread queue contains:
  // (active:)
  // 1. Wait (on db_event)
  // (pending:)
  // 2. "Init And Chain-Load First Domain"
  // 3. Priority Load (aaa.com)
  // 4. Wait (on db_event)
  db_thread_event_.Signal();
  key_loaded_event_.Wait();
  ASSERT_EQ(loaded_event_.IsSignaled(), false);
  std::set<std::string> cookies_loaded;
  for (CanonicalCookieVector::const_iterator it = cookies_.begin();
       it != cookies_.end();
       ++it) {
    cookies_loaded.insert((*it)->Domain().c_str());
  }
  STLDeleteElements(&cookies_);
  ASSERT_GT(4U, cookies_loaded.size());
  ASSERT_EQ(true, cookies_loaded.find("www.aaa.com") != cookies_loaded.end());
  ASSERT_EQ(true,
            cookies_loaded.find("travel.aaa.com") != cookies_loaded.end());

  db_thread_event_.Signal();
  loaded_event_.Wait();
  for (CanonicalCookieVector::const_iterator it = cookies_.begin();
       it != cookies_.end();
       ++it) {
    cookies_loaded.insert((*it)->Domain().c_str());
  }
  ASSERT_EQ(4U, cookies_loaded.size());
  ASSERT_EQ(cookies_loaded.find("foo.bar") != cookies_loaded.end(),
            true);
  ASSERT_EQ(cookies_loaded.find("www.bbb.com") != cookies_loaded.end(), true);
  STLDeleteElements(&cookies_);
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlush) {
  InitializeStore(false);
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  base::FilePath path = temp_dir_.path().Append(chrome::kCookieFilename);
  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  int64 base_size = info.size;

  // Write some large cookies, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::TimeDelta::FromMicroseconds(c);
    std::string name(1, c);
    std::string value(1000, c);
    AddCookie(name, value, "foo.bar", "/", t);
  }

  Flush();

  // We forced a write, so now the file will be bigger.
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  ASSERT_GT(info.size, base_size);
}

// Test loading old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestLoadOldSessionCookies) {
  InitializeStore(true);

  // Add a session cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), "C", "D", "sessioncookie.com", "/", std::string(),
          std::string(), base::Time::Now(), base::Time(),
          base::Time::Now(), false, false));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that loads session cookies and test that the session cookie
  // was loaded.
  CanonicalCookieVector cookies;
  CreateAndLoad(true, &cookies);

  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("sessioncookie.com", cookies[0]->Domain().c_str());
  ASSERT_STREQ("C", cookies[0]->Name().c_str());
  ASSERT_STREQ("D", cookies[0]->Value().c_str());

  STLDeleteElements(&cookies);
}

// Test loading old session cookies from the disk.
TEST_F(SQLitePersistentCookieStoreTest, TestDontLoadOldSessionCookies) {
  InitializeStore(true);

  // Add a session cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), "C", "D", "sessioncookie.com", "/", std::string(),
          std::string(), base::Time::Now(), base::Time(),
          base::Time::Now(), false, false));

  // Force the store to write its data to the disk.
  DestroyStore();

  // Create a store that doesn't load old session cookies and test that the
  // session cookie was not loaded.
  CanonicalCookieVector cookies;
  CreateAndLoad(false, &cookies);
  ASSERT_EQ(0U, cookies.size());

  // The store should also delete the session cookie. Wait until that has been
  // done.
  DestroyStore();

  // Create a store that loads old session cookies and test that the session
  // cookie is gone.
  CreateAndLoad(true, &cookies);
  ASSERT_EQ(0U, cookies.size());
}

TEST_F(SQLitePersistentCookieStoreTest, PersistIsPersistent) {
  InitializeStore(true);
  static const char kSessionName[] = "session";
  static const char kPersistentName[] = "persistent";

  // Add a session cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kSessionName, "val", "sessioncookie.com", "/",
          std::string(), std::string(),
          base::Time::Now(), base::Time(), base::Time::Now(),
          false, false));
  // Add a persistent cookie.
  store_->AddCookie(
      net::CanonicalCookie(
          GURL(), kPersistentName, "val", "sessioncookie.com", "/",
          std::string(), std::string(),
          base::Time::Now() - base::TimeDelta::FromDays(1), base::Time::Now(),
          base::Time::Now(), false, false));

  // Create a store that loads session cookie and test that the the IsPersistent
  // attribute is restored.
  CanonicalCookieVector cookies;
  CreateAndLoad(true, &cookies);
  ASSERT_EQ(2U, cookies.size());

  std::map<std::string, net::CanonicalCookie*> cookie_map;
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end();
       ++it) {
    cookie_map[(*it)->Name()] = *it;
  }

  std::map<std::string, net::CanonicalCookie*>::const_iterator it =
      cookie_map.find(kSessionName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_FALSE(cookie_map[kSessionName]->IsPersistent());

  it = cookie_map.find(kPersistentName);
  ASSERT_TRUE(it != cookie_map.end());
  EXPECT_TRUE(cookie_map[kPersistentName]->IsPersistent());

  STLDeleteElements(&cookies);
}
