// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/net/sqlite_origin_bound_cert_store.h"
#include "chrome/common/chrome_constants.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class SQLiteOriginBoundCertStoreTest : public testing::Test {
 public:
  SQLiteOriginBoundCertStoreTest()
      : db_thread_(BrowserThread::DB) {
  }

 protected:
  virtual void SetUp() {
    db_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLiteOriginBoundCertStore(
        temp_dir_.path().Append(chrome::kOBCertFilename));
    std::vector<net::DefaultOriginBoundCertStore::OriginBoundCert*> certs;
    ASSERT_TRUE(store_->Load(&certs));
    ASSERT_EQ(0u, certs.size());
    // Make sure the store gets written at least once.
    store_->AddOriginBoundCert(
        net::DefaultOriginBoundCertStore::OriginBoundCert(
            "https://encrypted.google.com:8443", "a", "b"));
  }

  content::TestBrowserThread db_thread_;
  ScopedTempDir temp_dir_;
  scoped_refptr<SQLiteOriginBoundCertStore> store_;
};

TEST_F(SQLiteOriginBoundCertStoreTest, KeepOnDestruction) {
  store_->SetClearLocalStateOnExit(false);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  ASSERT_TRUE(file_util::PathExists(
      temp_dir_.path().Append(chrome::kOBCertFilename)));
  ASSERT_TRUE(file_util::Delete(
      temp_dir_.path().Append(chrome::kOBCertFilename), false));
}

TEST_F(SQLiteOriginBoundCertStoreTest, RemoveOnDestruction) {
  store_->SetClearLocalStateOnExit(true);
  // Replace the store effectively destroying the current one and forcing it
  // to write it's data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  ASSERT_FALSE(file_util::PathExists(
      temp_dir_.path().Append(chrome::kOBCertFilename)));
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLiteOriginBoundCertStoreTest, TestPersistence) {
  std::vector<net::DefaultOriginBoundCertStore::OriginBoundCert*> certs;
  // Replace the store effectively destroying the current one and forcing it
  // to write it's data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  store_ = new SQLiteOriginBoundCertStore(
      temp_dir_.path().Append(chrome::kOBCertFilename));

  // Reload and test for persistence
  ASSERT_TRUE(store_->Load(&certs));
  ASSERT_EQ(1U, certs.size());
  ASSERT_STREQ("https://encrypted.google.com:8443", certs[0]->origin().c_str());
  ASSERT_STREQ("a", certs[0]->private_key().c_str());
  ASSERT_STREQ("b", certs[0]->cert().c_str());

  // Now delete the cert and check persistence again.
  store_->DeleteOriginBoundCert(*certs[0]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  STLDeleteContainerPointers(certs.begin(), certs.end());
  certs.clear();
  store_ = new SQLiteOriginBoundCertStore(
      temp_dir_.path().Append(chrome::kOBCertFilename));

  // Reload and check if the cert has been removed.
  ASSERT_TRUE(store_->Load(&certs));
  ASSERT_EQ(0U, certs.size());
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLiteOriginBoundCertStoreTest, TestFlush) {
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  FilePath path = temp_dir_.path().Append(chrome::kOBCertFilename);
  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  int64 base_size = info.size;

  // Write some certs, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    std::string origin(1, c);
    std::string private_key(1000, c);
    std::string cert(1000, c);
    store_->AddOriginBoundCert(
        net::DefaultOriginBoundCertStore::OriginBoundCert(origin,
                                                          private_key,
                                                          cert));
  }

  // Call Flush() and wait until the DB thread is idle.
  store_->Flush(base::Closure());
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
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
TEST_F(SQLiteOriginBoundCertStoreTest, TestFlushCompletionCallback) {
  scoped_refptr<CallbackCounter> counter(new CallbackCounter());

  // Callback shouldn't be invoked until we call Flush().
  ASSERT_EQ(0, counter->callback_count());

  store_->Flush(base::Bind(&CallbackCounter::Callback, counter.get()));

  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  ASSERT_EQ(1, counter->callback_count());
}
