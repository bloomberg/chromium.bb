// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/net/sqlite_origin_bound_cert_store.h"
#include "chrome/common/chrome_constants.h"
#include "content/test/test_browser_thread.h"
#include "net/base/cert_test_util.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class SQLiteOriginBoundCertStoreTest : public testing::Test {
 public:
  SQLiteOriginBoundCertStoreTest()
      : db_thread_(BrowserThread::DB) {
  }

 protected:
  static void ReadTestKeyAndCert(std::string* key, std::string* cert) {
    FilePath key_path = net::GetTestCertsDirectory().AppendASCII(
        "unittest.originbound.key.der");
    FilePath cert_path = net::GetTestCertsDirectory().AppendASCII(
        "unittest.originbound.der");
    ASSERT_TRUE(file_util::ReadFileToString(key_path, key));
    ASSERT_TRUE(file_util::ReadFileToString(cert_path, cert));
  }

  static base::Time GetTestCertExpirationTime() {
    // Cert expiration time from 'dumpasn1 unittest.originbound.der':
    // GeneralizedTime 19/11/2111 02:23:45 GMT
    // base::Time::FromUTCExploded can't generate values past 2038 on 32-bit
    // linux, so we use the raw value here.
    return base::Time::FromInternalValue(GG_INT64_C(16121816625000000));
  }

  virtual void SetUp() {
    db_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLiteOriginBoundCertStore(
        temp_dir_.path().Append(chrome::kOBCertFilename));
    ScopedVector<net::DefaultOriginBoundCertStore::OriginBoundCert> certs;
    ASSERT_TRUE(store_->Load(&certs.get()));
    ASSERT_EQ(0u, certs.size());
    // Make sure the store gets written at least once.
    store_->AddOriginBoundCert(
        net::DefaultOriginBoundCertStore::OriginBoundCert(
            "https://encrypted.google.com:8443",
            net::CLIENT_CERT_RSA_SIGN,
            base::Time(),
            "a", "b"));
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
  store_->AddOriginBoundCert(
      net::DefaultOriginBoundCertStore::OriginBoundCert(
          "https://www.google.com/",
          net::CLIENT_CERT_ECDSA_SIGN,
          base::Time(),
          "c", "d"));

  ScopedVector<net::DefaultOriginBoundCertStore::OriginBoundCert> certs;
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
  ASSERT_TRUE(store_->Load(&certs.get()));
  ASSERT_EQ(2U, certs.size());
  net::DefaultOriginBoundCertStore::OriginBoundCert* ec_cert;
  net::DefaultOriginBoundCertStore::OriginBoundCert* rsa_cert;
  if (net::CLIENT_CERT_RSA_SIGN == certs[0]->type()) {
    rsa_cert = certs[0];
    ec_cert = certs[1];
  } else {
    rsa_cert = certs[1];
    ec_cert = certs[0];
  }
  ASSERT_STREQ("https://encrypted.google.com:8443", rsa_cert->origin().c_str());
  ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, rsa_cert->type());
  ASSERT_STREQ("a", rsa_cert->private_key().c_str());
  ASSERT_STREQ("b", rsa_cert->cert().c_str());
  ASSERT_STREQ("https://www.google.com/", ec_cert->origin().c_str());
  ASSERT_EQ(net::CLIENT_CERT_ECDSA_SIGN, ec_cert->type());
  ASSERT_STREQ("c", ec_cert->private_key().c_str());
  ASSERT_STREQ("d", ec_cert->cert().c_str());

  // Now delete the cert and check persistence again.
  store_->DeleteOriginBoundCert(*certs[0]);
  store_->DeleteOriginBoundCert(*certs[1]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  certs.reset();
  store_ = new SQLiteOriginBoundCertStore(
      temp_dir_.path().Append(chrome::kOBCertFilename));

  // Reload and check if the cert has been removed.
  ASSERT_TRUE(store_->Load(&certs.get()));
  ASSERT_EQ(0U, certs.size());
}

TEST_F(SQLiteOriginBoundCertStoreTest, TestUpgradeV1) {
  // Reset the store.  We'll be using a different database for this test.
  store_ = NULL;

  FilePath v1_db_path(temp_dir_.path().AppendASCII("v1db"));

  std::string key_data;
  std::string cert_data;
  ReadTestKeyAndCert(&key_data, &cert_data);

  // Create a version 1 database.
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(v1_db_path));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
            "value LONGVARCHAR);"
        "INSERT INTO \"meta\" VALUES('version','1');"
        "INSERT INTO \"meta\" VALUES('last_compatible_version','1');"
        "CREATE TABLE origin_bound_certs ("
            "origin TEXT NOT NULL UNIQUE PRIMARY KEY,"
            "private_key BLOB NOT NULL,cert BLOB NOT NULL);"));

    sql::Statement add_smt(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs (origin, private_key, cert) "
        "VALUES (?,?,?)"));
    add_smt.BindString(0, "https://www.google.com:443");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
            "'https://foo.com',X'AA',X'BB');"
        ));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are stored and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<net::DefaultOriginBoundCertStore::OriginBoundCert> certs;
    store_ = new SQLiteOriginBoundCertStore(v1_db_path);

    // Load the database and ensure the certs can be read and are marked as RSA.
    ASSERT_TRUE(store_->Load(&certs.get()));
    ASSERT_EQ(2U, certs.size());

    ASSERT_STREQ("https://www.google.com:443", certs[0]->origin().c_str());
    ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, certs[0]->type());
    ASSERT_EQ(GetTestCertExpirationTime(),
              certs[0]->expiration_time());
    ASSERT_EQ(key_data, certs[0]->private_key());
    ASSERT_EQ(cert_data, certs[0]->cert());

    ASSERT_STREQ("https://foo.com", certs[1]->origin().c_str());
    ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, certs[1]->type());
    // Undecodable cert, expiration time will be uninitialized.
    ASSERT_EQ(base::Time(), certs[1]->expiration_time());
    ASSERT_STREQ("\xaa", certs[1]->private_key().c_str());
    ASSERT_STREQ("\xbb", certs[1]->cert().c_str());

    store_ = NULL;
    // Make sure we wait until the destructor has run.
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
    ASSERT_TRUE(helper->Run());

    // Verify the database version is updated.
    {
      sql::Connection db;
      ASSERT_TRUE(db.Open(v1_db_path));
      sql::Statement smt(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = \"version\""));
      ASSERT_TRUE(smt);
      ASSERT_TRUE(smt.Step());
      EXPECT_EQ(3, smt.ColumnInt(0));
      EXPECT_FALSE(smt.Step());
    }
  }
}

TEST_F(SQLiteOriginBoundCertStoreTest, TestUpgradeV2) {
  // Reset the store.  We'll be using a different database for this test.
  store_ = NULL;

  FilePath v2_db_path(temp_dir_.path().AppendASCII("v2db"));

  std::string key_data;
  std::string cert_data;
  ReadTestKeyAndCert(&key_data, &cert_data);

  // Create a version 2 database.
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(v2_db_path));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
            "value LONGVARCHAR);"
        "INSERT INTO \"meta\" VALUES('version','2');"
        "INSERT INTO \"meta\" VALUES('last_compatible_version','1');"
        "CREATE TABLE origin_bound_certs ("
            "origin TEXT NOT NULL UNIQUE PRIMARY KEY,"
            "private_key BLOB NOT NULL,"
            "cert BLOB NOT NULL,"
            "cert_type INTEGER);"
        ));

    sql::Statement add_smt(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs (origin, private_key, cert, cert_type) "
        "VALUES (?,?,?,?)"));
    add_smt.BindString(0, "https://www.google.com:443");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 1);
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
            "'https://foo.com',X'AA',X'BB',64);"
        ));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are saved and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<net::DefaultOriginBoundCertStore::OriginBoundCert> certs;
    store_ = new SQLiteOriginBoundCertStore(v2_db_path);

    // Load the database and ensure the certs can be read and are marked as RSA.
    ASSERT_TRUE(store_->Load(&certs.get()));
    ASSERT_EQ(2U, certs.size());

    ASSERT_STREQ("https://www.google.com:443", certs[0]->origin().c_str());
    ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, certs[0]->type());
    ASSERT_EQ(GetTestCertExpirationTime(),
              certs[0]->expiration_time());
    ASSERT_EQ(key_data, certs[0]->private_key());
    ASSERT_EQ(cert_data, certs[0]->cert());

    ASSERT_STREQ("https://foo.com", certs[1]->origin().c_str());
    ASSERT_EQ(net::CLIENT_CERT_ECDSA_SIGN, certs[1]->type());
    // Undecodable cert, expiration time will be uninitialized.
    ASSERT_EQ(base::Time(), certs[1]->expiration_time());
    ASSERT_STREQ("\xaa", certs[1]->private_key().c_str());
    ASSERT_STREQ("\xbb", certs[1]->cert().c_str());

    store_ = NULL;
    // Make sure we wait until the destructor has run.
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
    ASSERT_TRUE(helper->Run());

    // Verify the database version is updated.
    {
      sql::Connection db;
      ASSERT_TRUE(db.Open(v2_db_path));
      sql::Statement smt(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = \"version\""));
      ASSERT_TRUE(smt);
      ASSERT_TRUE(smt.Step());
      EXPECT_EQ(3, smt.ColumnInt(0));
      EXPECT_FALSE(smt.Step());
    }
  }
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
        net::DefaultOriginBoundCertStore::OriginBoundCert(
            origin,
            net::CLIENT_CERT_RSA_SIGN,
            base::Time(),
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
