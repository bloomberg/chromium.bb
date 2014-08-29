// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "net/base/test_data_directory.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/test/cert_test_util.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

const base::FilePath::CharType kTestChannelIDFilename[] =
    FILE_PATH_LITERAL("ChannelID");

class SQLiteChannelIDStoreTest : public testing::Test {
 public:
  void Load(ScopedVector<DefaultChannelIDStore::ChannelID>* channel_ids) {
    base::RunLoop run_loop;
    store_->Load(base::Bind(&SQLiteChannelIDStoreTest::OnLoaded,
                            base::Unretained(this),
                            &run_loop));
    run_loop.Run();
    channel_ids->swap(channel_ids_);
    channel_ids_.clear();
  }

  void OnLoaded(
      base::RunLoop* run_loop,
      scoped_ptr<ScopedVector<DefaultChannelIDStore::ChannelID> > channel_ids) {
    channel_ids_.swap(*channel_ids);
    run_loop->Quit();
  }

 protected:
  static void ReadTestKeyAndCert(std::string* key, std::string* cert) {
    base::FilePath key_path =
        GetTestCertsDirectory().AppendASCII("unittest.originbound.key.der");
    base::FilePath cert_path =
        GetTestCertsDirectory().AppendASCII("unittest.originbound.der");
    ASSERT_TRUE(base::ReadFileToString(key_path, key));
    ASSERT_TRUE(base::ReadFileToString(cert_path, cert));
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
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLiteChannelIDStore(
        temp_dir_.path().Append(kTestChannelIDFilename),
        base::MessageLoopProxy::current());
    ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
    Load(&channel_ids);
    ASSERT_EQ(0u, channel_ids.size());
    // Make sure the store gets written at least once.
    store_->AddChannelID(
        DefaultChannelIDStore::ChannelID("google.com",
                                         base::Time::FromInternalValue(1),
                                         base::Time::FromInternalValue(2),
                                         "a",
                                         "b"));
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLiteChannelIDStore> store_;
  ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids_;
};

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLiteChannelIDStoreTest, TestPersistence) {
  store_->AddChannelID(
      DefaultChannelIDStore::ChannelID("foo.com",
                                       base::Time::FromInternalValue(3),
                                       base::Time::FromInternalValue(4),
                                       "c",
                                       "d"));

  ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  store_ =
      new SQLiteChannelIDStore(temp_dir_.path().Append(kTestChannelIDFilename),
                               base::MessageLoopProxy::current());

  // Reload and test for persistence
  Load(&channel_ids);
  ASSERT_EQ(2U, channel_ids.size());
  DefaultChannelIDStore::ChannelID* goog_channel_id;
  DefaultChannelIDStore::ChannelID* foo_channel_id;
  if (channel_ids[0]->server_identifier() == "google.com") {
    goog_channel_id = channel_ids[0];
    foo_channel_id = channel_ids[1];
  } else {
    goog_channel_id = channel_ids[1];
    foo_channel_id = channel_ids[0];
  }
  ASSERT_EQ("google.com", goog_channel_id->server_identifier());
  ASSERT_STREQ("a", goog_channel_id->private_key().c_str());
  ASSERT_STREQ("b", goog_channel_id->cert().c_str());
  ASSERT_EQ(1, goog_channel_id->creation_time().ToInternalValue());
  ASSERT_EQ(2, goog_channel_id->expiration_time().ToInternalValue());
  ASSERT_EQ("foo.com", foo_channel_id->server_identifier());
  ASSERT_STREQ("c", foo_channel_id->private_key().c_str());
  ASSERT_STREQ("d", foo_channel_id->cert().c_str());
  ASSERT_EQ(3, foo_channel_id->creation_time().ToInternalValue());
  ASSERT_EQ(4, foo_channel_id->expiration_time().ToInternalValue());

  // Now delete the cert and check persistence again.
  store_->DeleteChannelID(*channel_ids[0]);
  store_->DeleteChannelID(*channel_ids[1]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  channel_ids.clear();
  store_ =
      new SQLiteChannelIDStore(temp_dir_.path().Append(kTestChannelIDFilename),
                               base::MessageLoopProxy::current());

  // Reload and check if the cert has been removed.
  Load(&channel_ids);
  ASSERT_EQ(0U, channel_ids.size());
  // Close the store.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLiteChannelIDStoreTest, TestDeleteAll) {
  store_->AddChannelID(
      DefaultChannelIDStore::ChannelID("foo.com",
                                       base::Time::FromInternalValue(3),
                                       base::Time::FromInternalValue(4),
                                       "c",
                                       "d"));

  ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  store_ =
      new SQLiteChannelIDStore(temp_dir_.path().Append(kTestChannelIDFilename),
                               base::MessageLoopProxy::current());

  // Reload and test for persistence
  Load(&channel_ids);
  ASSERT_EQ(2U, channel_ids.size());
  // DeleteAll except foo.com (shouldn't fail if one is missing either).
  std::list<std::string> delete_server_identifiers;
  delete_server_identifiers.push_back("google.com");
  delete_server_identifiers.push_back("missing.com");
  store_->DeleteAllInList(delete_server_identifiers);

  // Now check persistence again.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
  channel_ids.clear();
  store_ =
      new SQLiteChannelIDStore(temp_dir_.path().Append(kTestChannelIDFilename),
                               base::MessageLoopProxy::current());

  // Reload and check that only foo.com persisted in store.
  Load(&channel_ids);
  ASSERT_EQ(1U, channel_ids.size());
  ASSERT_EQ("foo.com", channel_ids[0]->server_identifier());
  // Close the store.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SQLiteChannelIDStoreTest, TestUpgradeV1) {
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
        "'foo.com',X'AA',X'BB');"));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are stored and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
    store_ =
        new SQLiteChannelIDStore(v1_db_path, base::MessageLoopProxy::current());

    // Load the database. Because the existing v1 certs are implicitly of type
    // RSA, which is unsupported, they're discarded.
    Load(&channel_ids);
    ASSERT_EQ(0U, channel_ids.size());

    store_ = NULL;
    base::RunLoop().RunUntilIdle();

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

TEST_F(SQLiteChannelIDStoreTest, TestUpgradeV2) {
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
        "cert_type INTEGER);"));

    sql::Statement add_smt(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs (origin, private_key, cert, cert_type) "
        "VALUES (?,?,?,?)"));
    add_smt.BindString(0, "google.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 64);
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
        "'foo.com',X'AA',X'BB',64);"));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are saved and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
    store_ =
        new SQLiteChannelIDStore(v2_db_path, base::MessageLoopProxy::current());

    // Load the database and ensure the certs can be read.
    Load(&channel_ids);
    ASSERT_EQ(2U, channel_ids.size());

    ASSERT_EQ("google.com", channel_ids[0]->server_identifier());
    ASSERT_EQ(GetTestCertExpirationTime(), channel_ids[0]->expiration_time());
    ASSERT_EQ(key_data, channel_ids[0]->private_key());
    ASSERT_EQ(cert_data, channel_ids[0]->cert());

    ASSERT_EQ("foo.com", channel_ids[1]->server_identifier());
    // Undecodable cert, expiration time will be uninitialized.
    ASSERT_EQ(base::Time(), channel_ids[1]->expiration_time());
    ASSERT_STREQ("\xaa", channel_ids[1]->private_key().c_str());
    ASSERT_STREQ("\xbb", channel_ids[1]->cert().c_str());

    store_ = NULL;
    // Make sure we wait until the destructor has run.
    base::RunLoop().RunUntilIdle();

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

TEST_F(SQLiteChannelIDStoreTest, TestUpgradeV3) {
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
        "expiration_time INTEGER);"));

    sql::Statement add_smt(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs (origin, private_key, cert, cert_type, "
        "expiration_time) VALUES (?,?,?,?,?)"));
    add_smt.BindString(0, "google.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 64);
    add_smt.BindInt64(4, 1000);
    ASSERT_TRUE(add_smt.Run());

    ASSERT_TRUE(db.Execute(
        "INSERT INTO \"origin_bound_certs\" VALUES("
        "'foo.com',X'AA',X'BB',64,2000);"));
  }

  // Load and test the DB contents twice.  First time ensures that we can use
  // the updated values immediately.  Second time ensures that the updated
  // values are saved and read correctly on next load.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);

    ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
    store_ =
        new SQLiteChannelIDStore(v3_db_path, base::MessageLoopProxy::current());

    // Load the database and ensure the certs can be read.
    Load(&channel_ids);
    ASSERT_EQ(2U, channel_ids.size());

    ASSERT_EQ("google.com", channel_ids[0]->server_identifier());
    ASSERT_EQ(1000, channel_ids[0]->expiration_time().ToInternalValue());
    ASSERT_EQ(GetTestCertCreationTime(), channel_ids[0]->creation_time());
    ASSERT_EQ(key_data, channel_ids[0]->private_key());
    ASSERT_EQ(cert_data, channel_ids[0]->cert());

    ASSERT_EQ("foo.com", channel_ids[1]->server_identifier());
    ASSERT_EQ(2000, channel_ids[1]->expiration_time().ToInternalValue());
    // Undecodable cert, creation time will be uninitialized.
    ASSERT_EQ(base::Time(), channel_ids[1]->creation_time());
    ASSERT_STREQ("\xaa", channel_ids[1]->private_key().c_str());
    ASSERT_STREQ("\xbb", channel_ids[1]->cert().c_str());

    store_ = NULL;
    // Make sure we wait until the destructor has run.
    base::RunLoop().RunUntilIdle();

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

TEST_F(SQLiteChannelIDStoreTest, TestRSADiscarded) {
  // Reset the store.  We'll be using a different database for this test.
  store_ = NULL;

  base::FilePath v4_db_path(temp_dir_.path().AppendASCII("v4dbrsa"));

  std::string key_data;
  std::string cert_data;
  ReadTestKeyAndCert(&key_data, &cert_data);

  // Create a version 4 database with a mix of RSA and ECDSA certs.
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(v4_db_path));
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY,"
        "value LONGVARCHAR);"
        "INSERT INTO \"meta\" VALUES('version','4');"
        "INSERT INTO \"meta\" VALUES('last_compatible_version','1');"
        "CREATE TABLE origin_bound_certs ("
        "origin TEXT NOT NULL UNIQUE PRIMARY KEY,"
        "private_key BLOB NOT NULL,"
        "cert BLOB NOT NULL,"
        "cert_type INTEGER,"
        "expiration_time INTEGER,"
        "creation_time INTEGER);"));

    sql::Statement add_smt(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs "
        "(origin, private_key, cert, cert_type, expiration_time, creation_time)"
        " VALUES (?,?,?,?,?,?)"));
    add_smt.BindString(0, "google.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 64);
    add_smt.BindInt64(4, GetTestCertExpirationTime().ToInternalValue());
    add_smt.BindInt64(5, base::Time::Now().ToInternalValue());
    ASSERT_TRUE(add_smt.Run());

    add_smt.Clear();
    add_smt.Assign(db.GetUniqueStatement(
        "INSERT INTO origin_bound_certs "
        "(origin, private_key, cert, cert_type, expiration_time, creation_time)"
        " VALUES (?,?,?,?,?,?)"));
    add_smt.BindString(0, "foo.com");
    add_smt.BindBlob(1, key_data.data(), key_data.size());
    add_smt.BindBlob(2, cert_data.data(), cert_data.size());
    add_smt.BindInt64(3, 1);
    add_smt.BindInt64(4, GetTestCertExpirationTime().ToInternalValue());
    add_smt.BindInt64(5, base::Time::Now().ToInternalValue());
    ASSERT_TRUE(add_smt.Run());
  }

  ScopedVector<DefaultChannelIDStore::ChannelID> channel_ids;
  store_ =
      new SQLiteChannelIDStore(v4_db_path, base::MessageLoopProxy::current());

  // Load the database and ensure the certs can be read.
  Load(&channel_ids);
  // Only the ECDSA cert (for google.com) is read, the RSA one is discarded.
  ASSERT_EQ(1U, channel_ids.size());

  ASSERT_EQ("google.com", channel_ids[0]->server_identifier());
  ASSERT_EQ(GetTestCertExpirationTime(), channel_ids[0]->expiration_time());
  ASSERT_EQ(key_data, channel_ids[0]->private_key());
  ASSERT_EQ(cert_data, channel_ids[0]->cert());

  store_ = NULL;
  // Make sure we wait until the destructor has run.
  base::RunLoop().RunUntilIdle();
}

}  // namespace net
