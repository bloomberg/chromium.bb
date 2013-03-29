// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/net/clear_on_exit_policy.h"
#include "chrome/browser/net/sqlite_server_bound_cert_store.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/test_data_directory.h"
#include "net/test/cert_test_util.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/quota/mock_special_storage_policy.h"

using content::BrowserThread;

class SQLiteServerBoundCertStoreTest : public testing::Test {
 public:
  SQLiteServerBoundCertStoreTest()
      : db_thread_(BrowserThread::DB),
        io_thread_(BrowserThread::IO, &message_loop_) {}

  void Load(
      ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert>* certs) {
    base::RunLoop run_loop;
    store_->Load(base::Bind(&SQLiteServerBoundCertStoreTest::OnLoaded,
                            base::Unretained(this),
                            &run_loop));
    run_loop.Run();
    certs->swap(certs_);
    certs_.clear();
  }

  void OnLoaded(
      base::RunLoop* run_loop,
      scoped_ptr<ScopedVector<
          net::DefaultServerBoundCertStore::ServerBoundCert> > certs) {
    certs_.swap(*certs);
    run_loop->Quit();
  }

 protected:
  static void ReadTestKeyAndCert(std::string* key, std::string* cert) {
    base::FilePath key_path = net::GetTestCertsDirectory().AppendASCII(
        "unittest.originbound.key.der");
    base::FilePath cert_path = net::GetTestCertsDirectory().AppendASCII(
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

  static base::Time GetTestCertCreationTime() {
    // UTCTime 13/12/2011 02:23:45 GMT
    base::Time::Exploded exploded_time;
    exploded_time.year = 2011;
    exploded_time.month = 12;
    exploded_time.day_of_week = 0;  // Unused.
    exploded_time.day_of_month = 13;
    exploded_time.hour = 2;
    exploded_time.minute = 23;
    exploded_time.second = 45;
    exploded_time.millisecond = 0;
    return base::Time::FromUTCExploded(exploded_time);
  }

  virtual void SetUp() {
    db_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLiteServerBoundCertStore(
        temp_dir_.path().Append(chrome::kOBCertFilename), NULL);
    ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> certs;
    Load(&certs);
    ASSERT_EQ(0u, certs.size());
    // Make sure the store gets written at least once.
    store_->AddServerBoundCert(
        net::DefaultServerBoundCertStore::ServerBoundCert(
            "google.com",
            net::CLIENT_CERT_RSA_SIGN,
            base::Time::FromInternalValue(1),
            base::Time::FromInternalValue(2),
            "a", "b"));
  }

  MessageLoopForIO message_loop_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread io_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLiteServerBoundCertStore> store_;
  ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> certs_;
};

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLiteServerBoundCertStoreTest, TestPersistence) {
  store_->AddServerBoundCert(
      net::DefaultServerBoundCertStore::ServerBoundCert(
          "foo.com",
          net::CLIENT_CERT_ECDSA_SIGN,
          base::Time::FromInternalValue(3),
          base::Time::FromInternalValue(4),
          "c", "d"));

  ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> certs;
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  store_ = new SQLiteServerBoundCertStore(
      temp_dir_.path().Append(chrome::kOBCertFilename), NULL);

  // Reload and test for persistence
  Load(&certs);
  ASSERT_EQ(2U, certs.size());
  net::DefaultServerBoundCertStore::ServerBoundCert* ec_cert;
  net::DefaultServerBoundCertStore::ServerBoundCert* rsa_cert;
  if (net::CLIENT_CERT_RSA_SIGN == certs[0]->type()) {
    rsa_cert = certs[0];
    ec_cert = certs[1];
  } else {
    rsa_cert = certs[1];
    ec_cert = certs[0];
  }
  ASSERT_STREQ("google.com", rsa_cert->server_identifier().c_str());
  ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, rsa_cert->type());
  ASSERT_STREQ("a", rsa_cert->private_key().c_str());
  ASSERT_STREQ("b", rsa_cert->cert().c_str());
  ASSERT_EQ(1, rsa_cert->creation_time().ToInternalValue());
  ASSERT_EQ(2, rsa_cert->expiration_time().ToInternalValue());
  ASSERT_STREQ("foo.com", ec_cert->server_identifier().c_str());
  ASSERT_EQ(net::CLIENT_CERT_ECDSA_SIGN, ec_cert->type());
  ASSERT_STREQ("c", ec_cert->private_key().c_str());
  ASSERT_STREQ("d", ec_cert->cert().c_str());
  ASSERT_EQ(3, ec_cert->creation_time().ToInternalValue());
  ASSERT_EQ(4, ec_cert->expiration_time().ToInternalValue());

  // Now delete the cert and check persistence again.
  store_->DeleteServerBoundCert(*certs[0]);
  store_->DeleteServerBoundCert(*certs[1]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  certs.clear();
  store_ = new SQLiteServerBoundCertStore(
      temp_dir_.path().Append(chrome::kOBCertFilename), NULL);

  // Reload and check if the cert has been removed.
  Load(&certs);
  ASSERT_EQ(0U, certs.size());
}

TEST_F(SQLiteServerBoundCertStoreTest, TestUpgradeV1) {
  // Reset the store.  We'll be using a different database for this test.
  store_ = NULL;

  base::FilePath v1_db_path(temp_dir_.path().AppendASCII("v1db"));

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
    add_smt.BindString(0, "google.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
            "'foo.com',X'AA',X'BB');"
        ));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are stored and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> certs;
    store_ = new SQLiteServerBoundCertStore(v1_db_path, NULL);

    // Load the database and ensure the certs can be read and are marked as RSA.
    Load(&certs);
    ASSERT_EQ(2U, certs.size());

    ASSERT_STREQ("google.com", certs[0]->server_identifier().c_str());
    ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, certs[0]->type());
    ASSERT_EQ(GetTestCertExpirationTime(),
              certs[0]->expiration_time());
    ASSERT_EQ(key_data, certs[0]->private_key());
    ASSERT_EQ(cert_data, certs[0]->cert());

    ASSERT_STREQ("foo.com", certs[1]->server_identifier().c_str());
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
      ASSERT_TRUE(smt.Step());
      EXPECT_EQ(4, smt.ColumnInt(0));
      EXPECT_FALSE(smt.Step());
    }
  }
}

TEST_F(SQLiteServerBoundCertStoreTest, TestUpgradeV2) {
  // Reset the store.  We'll be using a different database for this test.
  store_ = NULL;

  base::FilePath v2_db_path(temp_dir_.path().AppendASCII("v2db"));

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
    add_smt.BindString(0, "google.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 1);
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
            "'foo.com',X'AA',X'BB',64);"
        ));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are saved and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> certs;
    store_ = new SQLiteServerBoundCertStore(v2_db_path, NULL);

    // Load the database and ensure the certs can be read and are marked as RSA.
    Load(&certs);
    ASSERT_EQ(2U, certs.size());

    ASSERT_STREQ("google.com", certs[0]->server_identifier().c_str());
    ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, certs[0]->type());
    ASSERT_EQ(GetTestCertExpirationTime(),
              certs[0]->expiration_time());
    ASSERT_EQ(key_data, certs[0]->private_key());
    ASSERT_EQ(cert_data, certs[0]->cert());

    ASSERT_STREQ("foo.com", certs[1]->server_identifier().c_str());
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
      ASSERT_TRUE(smt.Step());
      EXPECT_EQ(4, smt.ColumnInt(0));
      EXPECT_FALSE(smt.Step());
    }
  }
}

TEST_F(SQLiteServerBoundCertStoreTest, TestUpgradeV3) {
  // Reset the store.  We'll be using a different database for this test.
  store_ = NULL;

  base::FilePath v3_db_path(temp_dir_.path().AppendASCII("v3db"));

  std::string key_data;
  std::string cert_data;
  ReadTestKeyAndCert(&key_data, &cert_data);

  // Create a version 3 database.
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(v3_db_path));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
            "value LONGVARCHAR);"
        "INSERT INTO \"meta\" VALUES('version','3');"
        "INSERT INTO \"meta\" VALUES('last_compatible_version','1');"
        "CREATE TABLE origin_bound_certs ("
            "origin TEXT NOT NULL UNIQUE PRIMARY KEY,"
            "private_key BLOB NOT NULL,"
            "cert BLOB NOT NULL,"
            "cert_type INTEGER,"
            "expiration_time INTEGER);"
        ));

    sql::Statement add_smt(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs (origin, private_key, cert, cert_type, "
        "expiration_time) VALUES (?,?,?,?,?)"));
    add_smt.BindString(0, "google.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 1);
    add_smt.BindInt64(4, 1000);
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
            "'foo.com',X'AA',X'BB',64,2000);"
        ));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are saved and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<net::DefaultServerBoundCertStore::ServerBoundCert> certs;
    store_ = new SQLiteServerBoundCertStore(v3_db_path, NULL);

    // Load the database and ensure the certs can be read and are marked as RSA.
    Load(&certs);
    ASSERT_EQ(2U, certs.size());

    ASSERT_STREQ("google.com", certs[0]->server_identifier().c_str());
    ASSERT_EQ(net::CLIENT_CERT_RSA_SIGN, certs[0]->type());
    ASSERT_EQ(1000, certs[0]->expiration_time().ToInternalValue());
    ASSERT_EQ(GetTestCertCreationTime(),
              certs[0]->creation_time());
    ASSERT_EQ(key_data, certs[0]->private_key());
    ASSERT_EQ(cert_data, certs[0]->cert());

    ASSERT_STREQ("foo.com", certs[1]->server_identifier().c_str());
    ASSERT_EQ(net::CLIENT_CERT_ECDSA_SIGN, certs[1]->type());
    ASSERT_EQ(2000, certs[1]->expiration_time().ToInternalValue());
    // Undecodable cert, creation time will be uninitialized.
    ASSERT_EQ(base::Time(), certs[1]->creation_time());
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
      ASSERT_TRUE(db.Open(v3_db_path));
      sql::Statement smt(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = \"version\""));
      ASSERT_TRUE(smt.Step());
      EXPECT_EQ(4, smt.ColumnInt(0));
      EXPECT_FALSE(smt.Step());
    }
  }
}
