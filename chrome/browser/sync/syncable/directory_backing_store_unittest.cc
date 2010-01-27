// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/common/sqlite_utils.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"

namespace syncable {

extern const int32 kCurrentDBVersion;

class MigrationTest : public testing::TestWithParam<int> {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  std::string GetUsername() {
    return "nick@chromium.org";
  }

  FilePath GetDatabasePath() {
    return temp_dir_.path().Append(
        DirectoryManager::GetSyncDataDatabaseFilename());
  }
  void SetUpVersion67Database();
  void SetUpVersion68Database();

 private:
  ScopedTempDir temp_dir_;
};

class DirectoryBackingStoreTest : public MigrationTest {};

void MigrationTest::SetUpVersion67Database() {
  // This is a version 67 database dump whose contents were backformed from
  // the contents of the version 68 database dump (the v68 migration was
  // actually written first).
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
          "CREATE TABLE metas (metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,"
          "ctime bigint default 0,server_ctime bigint default 0,"
          "server_position_in_parent bigint default 0,"
          "local_external_id bigint default 0,id varchar(255) default 'r',"
          "parent_id varchar(255) default 'r',"
          "server_parent_id varchar(255) default 'r',"
          "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
          "is_unsynced bit default 0,is_unapplied_update bit default 0,"
          "is_del bit default 0,is_dir bit default 0,"
          "is_bookmark_object bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,server_is_bookmark_object bit default 0,"
          "name varchar(255), "  /* COLLATE PATHNAME, */
          "unsanitized_name varchar(255)," /* COLLATE PATHNAME, */
          "non_unique_name varchar,"
          "server_name varchar(255),"  /* COLLATE PATHNAME */
          "server_non_unique_name varchar,"
          "bookmark_url varchar,server_bookmark_url varchar,"
          "singleton_tag varchar,bookmark_favicon blob,"
          "server_bookmark_favicon blob);"
      "INSERT INTO metas VALUES(1,-1,0,129079956640320000,0,"
          "129079956640320000,0,0,0,'r','r','r','r','r',0,0,0,1,0,0,0,0,NULL,"
          "NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,-2097152,"
          "4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,1,0,1,1,"
          "'Deleted Item',NULL,'Deleted Item','Deleted Item','Deleted Item',"
          "'http://www.google.com/','http://www.google.com/2',NULL,'AASGASGA',"
          "'ASADGADGADG');"
      "INSERT INTO metas VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,1,0,1,1,"
          "'Welcome to Chromium',NULL,'Welcome to Chromium',"
          "'Welcome to Chromium','Welcome to Chromium',"
          "'http://www.google.com/chrome/intl/en/welcome.html',"
          "'http://www.google.com/chrome/intl/en/welcome.html',NULL,NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,"
          "7,'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,1,0,1,1,"
          "'Google',NULL,'Google','Google','Google','http://www.google.com/',"
          "'http://www.google.com/',NULL,'AGASGASG','AGFDGASG');"
      "INSERT INTO metas VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,"
          "6,'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,1,0,1,"
          "'The Internet',NULL,'The Internet','The Internet',"
          "'The Internet',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,"
          "1048576,0,'s_ID_7','r','r','r','r',0,0,0,1,1,1,0,1,"
          "'Google Chrome',NULL,'Google Chrome','Google Chrome',"
          "'Google Chrome',NULL,NULL,'google_chrome',NULL,NULL);"
      "INSERT INTO metas VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,"
          "0,'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,1,0,1,'Bookmarks',"
          "NULL,'Bookmarks','Bookmarks','Bookmarks',NULL,NULL,"
          "'google_chrome_bookmarks',NULL,NULL);"
      "INSERT INTO metas VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,"
          "1048576,1,'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,1,0,"
          "1,'Bookmark Bar',NULL,'Bookmark Bar','Bookmark Bar','Bookmark Bar',"
          "NULL,NULL,'bookmark_bar',NULL,NULL);"
      "INSERT INTO metas VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,2097152,"
          "2,'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,1,0,1,"
          "'Other Bookmarks',NULL,'Other Bookmarks','Other Bookmarks',"
          "'Other Bookmarks',NULL,NULL,'other_bookmarks',"
          "NULL,NULL);"
      "INSERT INTO metas VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,1,0,0,1,"
          "'Home (The Chromium Projects)',NULL,'Home (The Chromium Projects)',"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "'http://dev.chromium.org/','http://dev.chromium.org/other',NULL,"
          "'AGATWA','AFAGVASF');"
      "INSERT INTO metas VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,1,0,1,"
          "'Extra Bookmarks',NULL,'Extra Bookmarks','Extra Bookmarks',"
          "'Extra Bookmarks',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,1,0,0,"
          "1,'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN  Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'http://www.icann.com/','http://www.icann.com/',NULL,"
          "'PNGAXF0AAFF','DAAFASF');"
      "INSERT INTO metas VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,"
          "11,'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,1,0,0,1,"
          "'The WebKit Open Source Project',NULL,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "'The WebKit Open Source Project','http://webkit.org/',"
          "'http://webkit.org/x',NULL,'PNGX','PNG2Y');"
      "CREATE TABLE share_info (id VARCHAR(128) primary key, "
          "last_sync_timestamp INT, name VARCHAR(128), "
          "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
          "db_create_version VARCHAR(128), db_create_time int, "
          "next_id bigint default -2, cache_guid VARCHAR(32));"
      "INSERT INTO share_info VALUES('nick@chromium.org',694,"
          "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
          "'Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO share_version VALUES('nick@chromium.org',68);"));
  ASSERT_TRUE(connection.CommitTransaction());
}

void MigrationTest::SetUpVersion68Database() {
  // This sets up an actual version 68 database dump.  The IDs were
  // canonicalized to be less huge, and the favicons were overwritten
  // with random junk so that they didn't contain any unprintable
  // characters.  A few server URLs were tweaked so that they'd be
  // different from the local URLs.  Lastly, the custom collation on
  // the server_non_unique_name column was removed.
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE metas (metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,"
          "ctime bigint default 0,server_ctime bigint default 0,"
          "server_position_in_parent bigint default 0,"
          "local_external_id bigint default 0,id varchar(255) default 'r',"
          "parent_id varchar(255) default 'r',"
          "server_parent_id varchar(255) default 'r',"
          "prev_id varchar(255) default 'r',next_id varchar(255) default 'r',"
          "is_unsynced bit default 0,is_unapplied_update bit default 0,"
          "is_del bit default 0,is_dir bit default 0,"
          "is_bookmark_object bit default 0,server_is_dir bit default 0,"
          "server_is_del bit default 0,"
          "server_is_bookmark_object bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "bookmark_url varchar,server_bookmark_url varchar,"
          "singleton_tag varchar,bookmark_favicon blob,"
          "server_bookmark_favicon blob);"
      "INSERT INTO metas VALUES(1,-1,0,129079956640320000,0,"
          "129079956640320000,0,0,0,'r','r','r','r','r',0,0,0,1,0,0,0,0,NULL,"
          "NULL,NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,-2097152,"
          "4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,1,0,1,1,"
          "'Deleted Item','Deleted Item','http://www.google.com/',"
          "'http://www.google.com/2',NULL,'AASGASGA','ASADGADGADG');"
      "INSERT INTO metas VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,1,0,1,1,"
          "'Welcome to Chromium','Welcome to Chromium',"
          "'http://www.google.com/chrome/intl/en/welcome.html',"
          "'http://www.google.com/chrome/intl/en/welcome.html',NULL,NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,"
          "7,'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,1,0,1,1,"
          "'Google','Google','http://www.google.com/',"
          "'http://www.google.com/',NULL,'AGASGASG','AGFDGASG');"
      "INSERT INTO metas VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,"
          "6,'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,1,0,1,"
          "'The Internet','The Internet',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,"
          "1048576,0,'s_ID_7','r','r','r','r',0,0,0,1,1,1,0,1,"
          "'Google Chrome','Google Chrome',NULL,NULL,'google_chrome',NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,"
          "0,'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,1,0,1,'Bookmarks',"
          "'Bookmarks',NULL,NULL,'google_chrome_bookmarks',NULL,NULL);"
      "INSERT INTO metas VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,"
          "1048576,1,'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,1,0,"
          "1,'Bookmark Bar','Bookmark Bar',NULL,NULL,'bookmark_bar',NULL,"
          "NULL);"
      "INSERT INTO metas VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,2097152,"
          "2,'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,1,0,1,"
          "'Other Bookmarks','Other Bookmarks',NULL,NULL,'other_bookmarks',"
          "NULL,NULL);"
      "INSERT INTO metas VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,1,0,0,1,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "'http://dev.chromium.org/','http://dev.chromium.org/other',NULL,"
          "'AGATWA','AFAGVASF');"
      "INSERT INTO metas VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,1,0,1,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,1,0,0,"
          "1,'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'http://www.icann.com/','http://www.icann.com/',NULL,"
          "'PNGAXF0AAFF','DAAFASF');"
      "INSERT INTO metas VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,"
          "11,'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,1,0,0,1,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "'http://webkit.org/','http://webkit.org/x',NULL,'PNGX','PNG2Y');"
      "CREATE TABLE share_info (id VARCHAR(128) primary key, "
          "last_sync_timestamp INT, name VARCHAR(128), "
          "initial_sync_ended BIT default 0, store_birthday VARCHAR(256), "
          "db_create_version VARCHAR(128), db_create_time int, "
          "next_id bigint default -2, cache_guid VARCHAR(32));"
      "INSERT INTO share_info VALUES('nick@chromium.org',694,"
          "'nick@chromium.org',1,'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb',"
          "'Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO share_version VALUES('nick@chromium.org',68);"));
  ASSERT_TRUE(connection.CommitTransaction());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion67To68) {
  SetUpVersion67Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing befor version 67.
    ASSERT_TRUE(connection.DoesColumnExist("metas", "name"));
    ASSERT_TRUE(connection.DoesColumnExist("metas", "unsanitized_name"));
    ASSERT_TRUE(connection.DoesColumnExist("metas", "server_name"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion67To68());
  ASSERT_EQ(68, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_TRUE(dbs->needs_column_refresh_);
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion68To69) {
  SetUpVersion68Database();

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion68To69());
  ASSERT_EQ(69, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_TRUE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.DoesColumnExist("metas", "specifics"));
  ASSERT_TRUE(connection.DoesColumnExist("metas", "server_specifics"));
  sql::Statement s(connection.GetUniqueStatement("SELECT non_unique_name,"
      "is_del, is_dir, id, specifics, server_specifics FROM metas "
      "WHERE metahandle = 2"));
  ASSERT_TRUE(s.Step());
  ASSERT_EQ("Deleted Item", s.ColumnString(0));
  ASSERT_TRUE(s.ColumnBool(1));
  ASSERT_FALSE(s.ColumnBool(2));
  ASSERT_EQ("s_ID_2", s.ColumnString(3));
  sync_pb::EntitySpecifics specifics;
  specifics.ParseFromArray(s.ColumnBlob(4), s.ColumnByteLength(4));
  ASSERT_TRUE(specifics.HasExtension(sync_pb::bookmark));
  ASSERT_EQ("http://www.google.com/",
      specifics.GetExtension(sync_pb::bookmark).url());
  ASSERT_EQ("AASGASGA", specifics.GetExtension(sync_pb::bookmark).favicon());
  specifics.ParseFromArray(s.ColumnBlob(5), s.ColumnByteLength(5));
  ASSERT_TRUE(specifics.HasExtension(sync_pb::bookmark));
  ASSERT_EQ("http://www.google.com/2",
      specifics.GetExtension(sync_pb::bookmark).url());
  ASSERT_EQ("ASADGADGADG", specifics.GetExtension(sync_pb::bookmark).favicon());
  ASSERT_FALSE(s.Step());
}

TEST_P(MigrationTest, ToCurrentVersion) {
  switch (GetParam()) {
    case 67:
      SetUpVersion67Database();
      break;
    case 68:
      SetUpVersion68Database();
      break;
    default:
      FAIL() << "Need to supply database dump for version " << GetParam();
  }

  scoped_ptr<DirectoryBackingStore> dbs(
    new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_TRUE(OPENED == dbs->InitializeTables());
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_EQ(kCurrentDBVersion, dbs->GetVersion());

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns deleted in Version 67.
    ASSERT_FALSE(connection.DoesColumnExist("metas", "name"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "unsanitized_name"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "server_name"));

    // Columns added in Version 68.
    ASSERT_TRUE(connection.DoesColumnExist("metas", "specifics"));
    ASSERT_TRUE(connection.DoesColumnExist("metas", "server_specifics"));

    // Columns deleted in Version 68.
    ASSERT_FALSE(connection.DoesColumnExist("metas", "is_bookmark_object"));
    ASSERT_FALSE(connection.DoesColumnExist("metas",
                                            "server_is_bookmark_object"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "bookmark_favicon"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "bookmark_url"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "server_bookmark_url"));
  }

  MetahandlesIndex index;
  dbs->LoadEntries(&index);
  dbs->EndLoad();

  MetahandlesIndex::iterator it = index.begin();
  ASSERT_TRUE(it != index.end());
  ASSERT_EQ(1, (*it)->ref(META_HANDLE));
  EXPECT_TRUE((*it)->ref(ID).IsRoot());

  ASSERT_TRUE(++it != index.end()) << "Upgrade destroyed database contents.";
  ASSERT_EQ(2, (*it)->ref(META_HANDLE));
  EXPECT_TRUE((*it)->ref(IS_DEL));
  EXPECT_TRUE((*it)->ref(SERVER_IS_DEL));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_EQ("http://www.google.com/",
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).url());
  EXPECT_EQ("AASGASGA",
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).favicon());
  EXPECT_EQ("http://www.google.com/2",
      (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).url());
  EXPECT_EQ("ASADGADGADG",
      (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).favicon());
  EXPECT_EQ("", (*it)->ref(SINGLETON_TAG));
  EXPECT_EQ("Deleted Item", (*it)->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Deleted Item", (*it)->ref(SERVER_NON_UNIQUE_NAME));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(4, (*it)->ref(META_HANDLE));
  EXPECT_TRUE((*it)->ref(IS_DEL));
  EXPECT_TRUE((*it)->ref(SERVER_IS_DEL));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(5, (*it)->ref(META_HANDLE));
  EXPECT_TRUE((*it)->ref(IS_DEL));
  EXPECT_TRUE((*it)->ref(SERVER_IS_DEL));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(6, (*it)->ref(META_HANDLE));
  EXPECT_TRUE((*it)->ref(IS_DIR));
  EXPECT_TRUE((*it)->ref(SERVER_IS_DIR));
  EXPECT_FALSE(
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).has_url());
  EXPECT_FALSE(
      (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).has_url());
  EXPECT_FALSE(
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).has_favicon());
  EXPECT_FALSE((*it)->ref(SERVER_SPECIFICS).
      GetExtension(sync_pb::bookmark).has_favicon());

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(7, (*it)->ref(META_HANDLE));
  EXPECT_EQ("google_chrome", (*it)->ref(SINGLETON_TAG));
  EXPECT_FALSE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_FALSE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(8, (*it)->ref(META_HANDLE));
  EXPECT_EQ("google_chrome_bookmarks", (*it)->ref(SINGLETON_TAG));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(9, (*it)->ref(META_HANDLE));
  EXPECT_EQ("bookmark_bar", (*it)->ref(SINGLETON_TAG));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(10, (*it)->ref(META_HANDLE));
  EXPECT_FALSE((*it)->ref(IS_DEL));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_FALSE((*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).has_url());
  EXPECT_FALSE(
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).has_favicon());
  EXPECT_FALSE(
      (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).has_url());
  EXPECT_FALSE((*it)->ref(SERVER_SPECIFICS).
      GetExtension(sync_pb::bookmark).has_favicon());
  EXPECT_EQ("other_bookmarks", (*it)->ref(SINGLETON_TAG));
  EXPECT_EQ("Other Bookmarks", (*it)->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Other Bookmarks", (*it)->ref(SERVER_NON_UNIQUE_NAME));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(11, (*it)->ref(META_HANDLE));
  EXPECT_FALSE((*it)->ref(IS_DEL));
  EXPECT_FALSE((*it)->ref(IS_DIR));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_EQ("http://dev.chromium.org/",
    (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).url());
  EXPECT_EQ("AGATWA",
    (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).favicon());
  EXPECT_EQ("http://dev.chromium.org/other",
    (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).url());
  EXPECT_EQ("AFAGVASF",
    (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).favicon());
  EXPECT_EQ("", (*it)->ref(SINGLETON_TAG));
  EXPECT_EQ("Home (The Chromium Projects)", (*it)->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Home (The Chromium Projects)", (*it)->ref(SERVER_NON_UNIQUE_NAME));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(12, (*it)->ref(META_HANDLE));
  EXPECT_FALSE((*it)->ref(IS_DEL));
  EXPECT_TRUE((*it)->ref(IS_DIR));
  EXPECT_EQ("Extra Bookmarks", (*it)->ref(NON_UNIQUE_NAME));
  EXPECT_EQ("Extra Bookmarks", (*it)->ref(SERVER_NON_UNIQUE_NAME));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_FALSE(
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).has_url());
  EXPECT_FALSE(
      (*it)->ref(SERVER_SPECIFICS).GetExtension(sync_pb::bookmark).has_url());
  EXPECT_FALSE(
      (*it)->ref(SPECIFICS).GetExtension(sync_pb::bookmark).has_favicon());
  EXPECT_FALSE((*it)->ref(SERVER_SPECIFICS).
      GetExtension(sync_pb::bookmark).has_favicon());

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(13, (*it)->ref(META_HANDLE));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(14, (*it)->ref(META_HANDLE));

  ASSERT_TRUE(++it == index.end());

  STLDeleteElements(&index);
}

INSTANTIATE_TEST_CASE_P(DirectoryBackingStore, MigrationTest,
                        testing::Range(67, kCurrentDBVersion));

}  // namespace syncable
