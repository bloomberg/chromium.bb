// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_backing_store.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable-inl.h"
#include "chrome/browser/sync/syncable/syncable.h"
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
  void SetUpVersion69Database();
  void SetUpVersion70Database();
  void SetUpVersion71Database();
  void SetUpVersion72Database();
  void SetUpVersion73Database();
  void SetUpVersion74Database();

  void SetUpCurrentDatabaseAndCheckVersion() {
    SetUpVersion70Database();  // Prepopulates data.
    scoped_ptr<DirectoryBackingStore> dbs(
        new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

    dbs->BeginLoad();
    ASSERT_EQ(OPENED, dbs->InitializeTables());
    ASSERT_FALSE(dbs->needs_column_refresh_);
    ASSERT_EQ(kCurrentDBVersion, dbs->GetVersion());
    dbs->EndLoad();
  }

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

void MigrationTest::SetUpVersion69Database() {
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
          "server_bookmark_favicon blob, specifics blob, "
          "server_specifics blob);"
      "INSERT INTO metas VALUES(1,-1,0,129079956640320000,0,129079956640320000,"
          "0,0,0,'r','r','r','r','r',0,0,0,1,0,0,0,0,NULL,NULL,NULL,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO metas VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,-2097152,"
          "4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,1,0,1,1,"
          "'Deleted Item','Deleted Item','http://www.google.com/',"
          "'http://www.google.com/2',NULL,'AASGASGA','ASADGADGADG',"
          "X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F120841415"
          "34741534741',X'C28810260A17687474703A2F2F7777772E676F6F676C652E636F"
          "6D2F32120B4153414447414447414447');"
      "INSERT INTO metas VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,1,0,1,1,"
          "'Welcome to Chromium','Welcome to Chromium',"
          "'http://www.google.com/chrome/intl/en/welcome.html',"
          "'http://www.google.com/chrome/intl/en/welcome.html',NULL,NULL,NULL,"
          "X'C28810350A31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6"
          "D652F696E746C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A3168"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F6"
          "56E2F77656C636F6D652E68746D6C1200');"
      "INSERT INTO metas VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,1,0,1,1,"
          "'Google','Google','http://www.google.com/',"
          "'http://www.google.com/',NULL,'AGASGASG','AGFDGASG',X'C28810220A166"
          "87474703A2F2F7777772E676F6F676C652E636F6D2F12084147415347415347',X'"
          "C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F12084147464"
          "447415347');"
      "INSERT INTO metas VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,6"
          ",'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,1,0,1,'The Internet',"
          "'The Internet',NULL,NULL,NULL,NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,1,0,1,'Google Chrome',"
          "'Google Chrome',NULL,NULL,'google_chrome',NULL,NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,1,0,1,'Bookmarks',"
          "'Bookmarks',NULL,NULL,'google_chrome_bookmarks',NULL,NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,1,0,1,"
          "'Bookmark Bar','Bookmark Bar',NULL,NULL,'bookmark_bar',NULL,NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,1,0,1,"
          "'Other Bookmarks','Other Bookmarks',NULL,NULL,'other_bookmarks',"
          "NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,1,0,0,1,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "'http://dev.chromium.org/','http://dev.chromium.org/other',NULL,"
          "'AGATWA','AFAGVASF',X'C28810220A18687474703A2F2F6465762E6368726F6D6"
          "9756D2E6F72672F1206414741545741',X'C28810290A1D687474703A2F2F646576"
          "2E6368726F6D69756D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO metas VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,1,0,1,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,NULL,NULL,NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,1,0,0,"
          "1,'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'http://www.icann.com/','http://www.icann.com/',NULL,'PNGAXF0AAFF',"
          "'DAAFASF',X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F1"
          "20B504E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963"
          "616E6E2E636F6D2F120744414146415346');"
      "INSERT INTO metas VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,1,0,0,1,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "'http://webkit.org/','http://webkit.org/x',NULL,'PNGX','PNG2Y',"
          "X'C288101A0A12687474703A2F2F7765626B69742E6F72672F1204504E4758',X'C2"
          "88101C0A13687474703A2F2F7765626B69742E6F72672F781205504E473259');"
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
      "INSERT INTO share_version VALUES('nick@chromium.org',69);"
  ));
  ASSERT_TRUE(connection.CommitTransaction());
}

void MigrationTest::SetUpVersion70Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
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
      "INSERT INTO share_version VALUES('nick@chromium.org',70);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
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
          "server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,"
          "specifics blob,server_specifics blob);"
      "INSERT INTO metas VALUES(1,-1,0,129079956640320000,0,129079956640320000,"
          "0,0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'');"
      "INSERT INTO metas VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,"
          "-2097152,4,'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,"
          "1,'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A"
          "2F2F7777772E676F6F676C652E636F6D2F12084141534741534741',X'C2881026"
          "0A17687474703A2F2F7777772E676F6F676C652E636F6D2F32120B415341444741"
          "4447414447');"
      "INSERT INTO metas VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,"
          "3,'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A"
          "31687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E74"
          "6C2F656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F"
          "2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F7765"
          "6C636F6D652E68746D6C1200');"
      "INSERT INTO metas VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C6"
          "52E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F77777"
          "72E676F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO metas VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,"
          "6,'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome',"
          "'Google Chrome','google_chrome',NULL,NULL,NULL);"
      "INSERT INTO metas VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO metas VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,"
          "1,'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,"
          "'Bookmark Bar','Bookmark Bar','bookmark_bar',NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO metas VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,"
          "2097152,2,'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO metas VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,"
          "8,'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',"
          "NULL,NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F"
          "72672F1206414741545741',X'C28810290A1D687474703A2F2F6465762E636872"
          "6F6D69756D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO metas VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO metas VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,"
          "10,'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "NULL,NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F"
          "120B504E474158463041414646',X'C28810200A15687474703A2F2F7777772E69"
          "63616E6E2E636F6D2F120744414146415346');"
      "INSERT INTO metas VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,"
          "11,'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F78120550"
          "4E473259');"
      ));
  ASSERT_TRUE(connection.CommitTransaction());
}

void MigrationTest::SetUpVersion71Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE extended_attributes(metahandle bigint, key varchar(127), "
          "value blob, PRIMARY KEY(metahandle, key) ON CONFLICT REPLACE);"
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',71);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,ctime bigint "
          "default 0,server_ctime bigint default 0,server_position_in_parent "
          "bigint default 0,local_external_id bigint default 0,id varchar(255) "
          "default 'r',parent_id varchar(255) default 'r',server_parent_id "
          "varchar(255) default 'r',prev_id varchar(255) default 'r',next_id "
          "varchar(255) default 'r',is_unsynced bit default 0,"
          "is_unapplied_update bit default 0,is_del bit default 0,is_dir bit "
          "default 0,server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,specifics blob,"
          "server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,129079956640320000,0,"
          "129079956640320000,0,0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,-2097152,4,"
          "'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,"
          "'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F"
          "7777772E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A1768"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474144"
          "47');"
      "INSERT INTO 'metas' VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,3,"
          "'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A31"
          "687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F"
          "656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7777"
          "772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D"
          "652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652"
          "E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,6,"
          "'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome'"
          ",'google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar',"
          "'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,8,"
          "'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',NULL,"
          "NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1"
          "206414741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,10,"
          "'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',NULL,"
          "NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
          "E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963616E6E2"
          "E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,""X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E"
          "473259');"
      "CREATE TABLE models (model_id BLOB primary key, "
          "last_download_timestamp INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, "
          "store_birthday TEXT, db_create_version TEXT, db_create_time INT, "
          "next_id INT default -2, cache_guid TEXT);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"));
  ASSERT_TRUE(connection.CommitTransaction());
}

void MigrationTest::SetUpVersion72Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',72);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,ctime bigint "
          "default 0,server_ctime bigint default 0,server_position_in_parent "
          "bigint default 0,local_external_id bigint default 0,id varchar(255) "
          "default 'r',parent_id varchar(255) default 'r',server_parent_id "
          "varchar(255) default 'r',prev_id varchar(255) default 'r',next_id "
          "varchar(255) default 'r',is_unsynced bit default 0,"
          "is_unapplied_update bit default 0,is_del bit default 0,is_dir bit "
          "default 0,server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,specifics blob,"
          "server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,129079956640320000,0,"
          "129079956640320000,0,0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,-2097152,4,"
          "'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,"
          "'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F"
          "7777772E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A1768"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474144"
          "47');"
      "INSERT INTO 'metas' VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,3,"
          "'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A31"
          "687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F"
          "656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7777"
          "772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D"
          "652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652"
          "E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,6,"
          "'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome'"
          ",'google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar',"
          "'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,8,"
          "'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',NULL,"
          "NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1"
          "206414741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,10,"
          "'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',NULL,"
          "NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
          "E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963616E6E2"
          "E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,""X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E"
          "473259');"
      "CREATE TABLE models (model_id BLOB primary key, "
          "last_download_timestamp INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, "
          "store_birthday TEXT, db_create_version TEXT, db_create_time INT, "
          "next_id INT default -2, cache_guid TEXT);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x');"));
  ASSERT_TRUE(connection.CommitTransaction());
}

void MigrationTest::SetUpVersion73Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',73);"
      "CREATE TABLE metas(metahandle bigint primary key ON CONFLICT FAIL,"
          "base_version bigint default -1,server_version bigint default 0,"
          "mtime bigint default 0,server_mtime bigint default 0,ctime bigint "
          "default 0,server_ctime bigint default 0,server_position_in_parent "
          "bigint default 0,local_external_id bigint default 0,id varchar(255) "
          "default 'r',parent_id varchar(255) default 'r',server_parent_id "
          "varchar(255) default 'r',prev_id varchar(255) default 'r',next_id "
          "varchar(255) default 'r',is_unsynced bit default 0,"
          "is_unapplied_update bit default 0,is_del bit default 0,is_dir bit "
          "default 0,server_is_dir bit default 0,server_is_del bit default 0,"
          "non_unique_name varchar,server_non_unique_name varchar(255),"
          "unique_server_tag varchar,unique_client_tag varchar,specifics blob,"
          "server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,129079956640320000,0,"
          "129079956640320000,0,0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,"
          "NULL,NULL,X'',X'');"
      "INSERT INTO 'metas' VALUES(2,669,669,128976886618480000,"
          "128976886618480000,128976886618480000,128976886618480000,-2097152,4,"
          "'s_ID_2','s_ID_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,"
          "'Deleted Item','Deleted Item',NULL,NULL,X'C28810220A16687474703A2F2F"
          "7777772E676F6F676C652E636F6D2F12084141534741534741',X'C28810260A1768"
          "7474703A2F2F7777772E676F6F676C652E636F6D2F32120B41534144474144474144"
          "47');"
      "INSERT INTO 'metas' VALUES(4,681,681,129002163642690000,"
          "129002163642690000,129002163642690000,129002163642690000,-3145728,3,"
          "'s_ID_4','s_ID_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,"
          "'Welcome to Chromium','Welcome to Chromium',NULL,NULL,X'C28810350A31"
          "687474703A2F2F7777772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F"
          "656E2F77656C636F6D652E68746D6C1200',X'C28810350A31687474703A2F2F7777"
          "772E676F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D"
          "652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677,129001555500000000,"
          "129001555500000000,129001555500000000,129001555500000000,1048576,7,"
          "'s_ID_5','s_ID_9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google',"
          "'Google',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652"
          "E636F6D2F12084147415347415347',X'C28810220A16687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694,129053976170000000,"
          "129053976170000000,129053976170000000,129053976170000000,-4194304,6,"
          "'s_ID_6','s_ID_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet',"
          "'The Internet',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_7','r','r','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome'"
          ",'google_chrome',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,0,"
          "'s_ID_8','s_ID_7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks',"
          "'Bookmarks','google_chrome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,1048576,1,"
          "'s_ID_9','s_ID_8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar',"
          "'Bookmark Bar','bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666,128976864758480000,"
          "128976864758480000,128976864758480000,128976864758480000,2097152,2,"
          "'s_ID_10','s_ID_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,"
          "'Other Bookmarks','Other Bookmarks','other_bookmarks',NULL,"
          "X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683,129079956948440000,"
          "129079956948440000,129079956948440000,129079956948440000,-1048576,8,"
          "'s_ID_11','s_ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,"
          "'Home (The Chromium Projects)','Home (The Chromium Projects)',NULL,"
          "NULL,X'C28810220A18687474703A2F2F6465762E6368726F6D69756D2E6F72672F1"
          "206414741545741',X'C28810290A1D687474703A2F2F6465762E6368726F6D69756"
          "D2E6F72672F6F7468657212084146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685,129079957513650000,"
          "129079957513650000,129079957513650000,129079957513650000,0,9,"
          "'s_ID_12','s_ID_6','s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,"
          "'Extra Bookmarks','Extra Bookmarks',NULL,NULL,X'C2881000',"
          "X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687,129079957985300000,"
          "129079957985300000,129079957985300000,129079957985300000,-917504,10,"
          "'s_ID_13','s_ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',"
          "'ICANN | Internet Corporation for Assigned Names and Numbers',NULL,"
          "NULL,X'C28810240A15687474703A2F2F7777772E6963616E6E2E636F6D2F120B504"
          "E474158463041414646',X'C28810200A15687474703A2F2F7777772E6963616E6E2"
          "E636F6D2F120744414146415346');"
      "INSERT INTO 'metas' VALUES(14,692,692,129079958383000000,"
          "129079958383000000,129079958383000000,129079958383000000,1048576,11,"
          "'s_ID_14','s_ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,"
          "'The WebKit Open Source Project','The WebKit Open Source Project',"
          "NULL,NULL,""X'C288101A0A12687474703A2F2F7765626B69742E6F72672F120450"
          "4E4758',X'C288101C0A13687474703A2F2F7765626B69742E6F72672F781205504E"
          "473259');"
      "CREATE TABLE models (model_id BLOB primary key, "
          "last_download_timestamp INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, "
          "store_birthday TEXT, db_create_version TEXT, db_create_time INT, "
          "next_id INT default -2, cache_guid TEXT, "
          "notification_state BLOB);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org',"
          "'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542,"
          "'9010788312004066376x-6609234393368420856x',X'C2881000');"));
  ASSERT_TRUE(connection.CommitTransaction());
}

void MigrationTest::SetUpVersion74Database() {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute(
      "CREATE TABLE share_version (id VARCHAR(128) primary key, data INT);"
      "INSERT INTO 'share_version' VALUES('nick@chromium.org',74);"
      "CREATE TABLE models (model_id BLOB primary key, last_download_timestamp"
          " INT, initial_sync_ended BOOLEAN default 0);"
      "INSERT INTO 'models' VALUES(X'C2881000',694,1);"
      "CREATE TABLE 'share_info' (id TEXT primary key, name TEXT, store_birthd"
          "ay TEXT, db_create_version TEXT, db_create_time INT, next_id INT de"
          "fault -2, cache_guid TEXT , notification_state BLOB, autofill_migra"
          "tion_state INT default 0, bookmarks_added_during_autofill_migration"
          " INT default 0, autofill_migration_time INT default 0, autofill_ent"
          "ries_added_during_migration INT default 0, autofill_profiles_added_"
          "during_migration INT default 0);"
      "INSERT INTO 'share_info' VALUES('nick@chromium.org','nick@chromium.org'"
          ",'c27e9f59-08ca-46f8-b0cc-f16a2ed778bb','Unknown',1263522064,-65542"
          ",'9010788312004066376x-6609234393368420856x',NULL,0,0,0,0,0);"
      "CREATE TABLE 'metas'(metahandle bigint primary key ON CONFLICT FAIL,bas"
          "e_version bigint default -1,server_version bigint default 0,mtime b"
          "igint default 0,server_mtime bigint default 0,ctime bigint default "
          "0,server_ctime bigint default 0,server_position_in_parent bigint de"
          "fault 0,local_external_id bigint default 0,id varchar(255) default "
          "'r',parent_id varchar(255) default 'r',server_parent_id varchar(255"
          ") default 'r',prev_id varchar(255) default 'r',next_id varchar(255)"
          " default 'r',is_unsynced bit default 0,is_unapplied_update bit defa"
          "ult 0,is_del bit default 0,is_dir bit default 0,server_is_dir bit d"
          "efault 0,server_is_del bit default 0,non_unique_name varchar,server"
          "_non_unique_name varchar(255),unique_server_tag varchar,unique_clie"
          "nt_tag varchar,specifics blob,server_specifics blob);"
      "INSERT INTO 'metas' VALUES(1,-1,0,129079956640320000,0,1290799566403200"
          "00,0,0,0,'r','r','r','r','r',0,0,0,1,0,0,NULL,NULL,NULL,NULL,X'',X'"
          "');"
      "INSERT INTO 'metas' VALUES(2,669,669,128976886618480000,128976886618480"
          "000,128976886618480000,128976886618480000,-2097152,4,'s_ID_2','s_ID"
          "_9','s_ID_9','s_ID_2','s_ID_2',0,0,1,0,0,1,'Deleted Item','Deleted "
          "Item',NULL,NULL,X'C28810220A16687474703A2F2F7777772E676F6F676C652E6"
          "36F6D2F12084141534741534741',X'C28810260A17687474703A2F2F7777772E67"
          "6F6F676C652E636F6D2F32120B4153414447414447414447');"
      "INSERT INTO 'metas' VALUES(4,681,681,129002163642690000,129002163642690"
          "000,129002163642690000,129002163642690000,-3145728,3,'s_ID_4','s_ID"
          "_9','s_ID_9','s_ID_4','s_ID_4',0,0,1,0,0,1,'Welcome to Chromium','W"
          "elcome to Chromium',NULL,NULL,X'C28810350A31687474703A2F2F7777772E6"
          "76F6F676C652E636F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D652E"
          "68746D6C1200',X'C28810350A31687474703A2F2F7777772E676F6F676C652E636"
          "F6D2F6368726F6D652F696E746C2F656E2F77656C636F6D652E68746D6C1200');"
      "INSERT INTO 'metas' VALUES(5,677,677,129001555500000000,129001555500000"
          "000,129001555500000000,129001555500000000,1048576,7,'s_ID_5','s_ID_"
          "9','s_ID_9','s_ID_5','s_ID_5',0,0,1,0,0,1,'Google','Google',NULL,NU"
          "LL,X'C28810220A16687474703A2F2F7777772E676F6F676C652E636F6D2F120841"
          "47415347415347',X'C28810220A16687474703A2F2F7777772E676F6F676C652E6"
          "36F6D2F12084147464447415347');"
      "INSERT INTO 'metas' VALUES(6,694,694,129053976170000000,129053976170000"
          "000,129053976170000000,129053976170000000,-4194304,6,'s_ID_6','s_ID"
          "_9','s_ID_9','r','r',0,0,0,1,1,0,'The Internet','The Internet',NULL"
          ",NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(7,663,663,128976864758480000,128976864758480"
          "000,128976864758480000,128976864758480000,1048576,0,'s_ID_7','r','r"
          "','r','r',0,0,0,1,1,0,'Google Chrome','Google Chrome','google_chrom"
          "e',NULL,NULL,NULL);"
      "INSERT INTO 'metas' VALUES(8,664,664,128976864758480000,128976864758480"
          "000,128976864758480000,128976864758480000,1048576,0,'s_ID_8','s_ID_"
          "7','s_ID_7','r','r',0,0,0,1,1,0,'Bookmarks','Bookmarks','google_chr"
          "ome_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(9,665,665,128976864758480000,128976864758480"
          "000,128976864758480000,128976864758480000,1048576,1,'s_ID_9','s_ID_"
          "8','s_ID_8','r','s_ID_10',0,0,0,1,1,0,'Bookmark Bar','Bookmark Bar'"
          ",'bookmark_bar',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(10,666,666,128976864758480000,12897686475848"
          "0000,128976864758480000,128976864758480000,2097152,2,'s_ID_10','s_I"
          "D_8','s_ID_8','s_ID_9','r',0,0,0,1,1,0,'Other Bookmarks','Other Boo"
          "kmarks','other_bookmarks',NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(11,683,683,129079956948440000,12907995694844"
          "0000,129079956948440000,129079956948440000,-1048576,8,'s_ID_11','s_"
          "ID_6','s_ID_6','r','s_ID_13',0,0,0,0,0,0,'Home (The Chromium Projec"
          "ts)','Home (The Chromium Projects)',NULL,NULL,X'C28810220A186874747"
          "03A2F2F6465762E6368726F6D69756D2E6F72672F1206414741545741',X'C28810"
          "290A1D687474703A2F2F6465762E6368726F6D69756D2E6F72672F6F74686572120"
          "84146414756415346');"
      "INSERT INTO 'metas' VALUES(12,685,685,129079957513650000,12907995751365"
          "0000,129079957513650000,129079957513650000,0,9,'s_ID_12','s_ID_6','"
          "s_ID_6','s_ID_13','s_ID_14',0,0,0,1,1,0,'Extra Bookmarks','Extra Bo"
          "okmarks',NULL,NULL,X'C2881000',X'C2881000');"
      "INSERT INTO 'metas' VALUES(13,687,687,129079957985300000,12907995798530"
          "0000,129079957985300000,129079957985300000,-917504,10,'s_ID_13','s_"
          "ID_6','s_ID_6','s_ID_11','s_ID_12',0,0,0,0,0,0,'ICANN | Internet Co"
          "rporation for Assigned Names and Numbers','ICANN | Internet Corpora"
          "tion for Assigned Names and Numbers',NULL,NULL,X'C28810240A15687474"
          "703A2F2F7777772E6963616E6E2E636F6D2F120B504E474158463041414646',X'C"
          "28810200A15687474703A2F2F7777772E6963616E6E2E636F6D2F12074441414641"
          "5346');"
      "INSERT INTO 'metas' VALUES(14,692,692,129079958383000000,12907995838300"
          "0000,129079958383000000,129079958383000000,1048576,11,'s_ID_14','s_"
          "ID_6','s_ID_6','s_ID_12','r',0,0,0,0,0,0,'The WebKit Open Source Pr"
          "oject','The WebKit Open Source Project',NULL,NULL,X'C288101A0A12687"
          "474703A2F2F7765626B69742E6F72672F1204504E4758',X'C288101C0A13687474"
          "703A2F2F7765626B69742E6F72672F781205504E473259');"
      ));
  ASSERT_TRUE(connection.CommitTransaction());
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion67To68) {
  SetUpVersion67Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing before version 67.
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

TEST_F(DirectoryBackingStoreTest, MigrateVersion69To70) {
  SetUpVersion69Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_TRUE(connection.DoesColumnExist("metas", "singleton_tag"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "unique_server_tag"));
    ASSERT_FALSE(connection.DoesColumnExist("metas", "unique_client_tag"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion69To70());
  ASSERT_EQ(70, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_TRUE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));

  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_server_tag"));
  EXPECT_TRUE(connection.DoesColumnExist("metas", "unique_client_tag"));
  sql::Statement s(connection.GetUniqueStatement("SELECT id"
      " FROM metas WHERE unique_server_tag = 'google_chrome'"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ("s_ID_7", s.ColumnString(0));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion70To71) {
  SetUpVersion70Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(
        connection.DoesColumnExist("share_info", "last_sync_timestamp"));
    ASSERT_TRUE(
        connection.DoesColumnExist("share_info", "initial_sync_ended"));
    ASSERT_FALSE(connection.DoesTableExist("models"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion70To71());
  ASSERT_EQ(71, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));

  ASSERT_FALSE(
      connection.DoesColumnExist("share_info", "last_sync_timestamp"));
  ASSERT_FALSE(
      connection.DoesColumnExist("share_info", "initial_sync_ended"));
  ASSERT_TRUE(connection.DoesTableExist("models"));
  ASSERT_TRUE(
      connection.DoesColumnExist("models", "initial_sync_ended"));
  ASSERT_TRUE(
      connection.DoesColumnExist("models", "last_download_timestamp"));
  ASSERT_TRUE(
      connection.DoesColumnExist("models", "model_id"));

  sql::Statement s(connection.GetUniqueStatement("SELECT model_id, "
      "initial_sync_ended, last_download_timestamp FROM models"));
  ASSERT_TRUE(s.Step());
  std::string model_id = s.ColumnString(0);
  EXPECT_EQ("C2881000", base::HexEncode(model_id.data(), model_id.size()))
      << "Model ID is expected to be the empty BookmarkSpecifics proto.";
  EXPECT_TRUE(s.ColumnBool(1));
  EXPECT_EQ(694, s.ColumnInt64(2));
  ASSERT_FALSE(s.Step());
}


TEST_F(DirectoryBackingStoreTest, MigrateVersion71To72) {
  SetUpVersion71Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(connection.DoesTableExist("extended_attributes"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion71To72());
  ASSERT_EQ(72, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_FALSE(connection.DoesTableExist("extended_attributes"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion72To73) {
  SetUpVersion72Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_FALSE(
        connection.DoesColumnExist("share_info", "notification_state"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion72To73());
  ASSERT_EQ(73, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(
      connection.DoesColumnExist("share_info", "notification_state"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion73To74) {
  SetUpVersion73Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_FALSE(
        connection.DoesColumnExist("share_info", "autofill_migration_state"));
    ASSERT_FALSE(
        connection.DoesColumnExist("share_info",
            "bookmarks_added_during_autofill_migration"));
    ASSERT_FALSE(
        connection.DoesColumnExist("share_info", "autofill_migration_time"));
    ASSERT_FALSE(
        connection.DoesColumnExist("share_info",
            "autofill_entries_added_during_migration"));

    ASSERT_FALSE(
        connection.DoesColumnExist("share_info",
            "autofill_profiles_added_during_migration"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion73To74());
  ASSERT_EQ(74, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(
      connection.DoesColumnExist("share_info", "autofill_migration_state"));
  ASSERT_TRUE(
      connection.DoesColumnExist("share_info",
          "bookmarks_added_during_autofill_migration"));
  ASSERT_TRUE(
      connection.DoesColumnExist("share_info", "autofill_migration_time"));
  ASSERT_TRUE(
      connection.DoesColumnExist("share_info",
          "autofill_entries_added_during_migration"));

  ASSERT_TRUE(
      connection.DoesColumnExist("share_info",
          "autofill_profiles_added_during_migration"));
}

TEST_F(DirectoryBackingStoreTest, MigrateVersion74To75) {
  SetUpVersion74Database();

  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_FALSE(connection.DoesColumnExist("models", "progress_marker"));
    ASSERT_TRUE(connection.DoesColumnExist("models",
        "last_download_timestamp"));
  }

  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

  dbs->BeginLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);
  ASSERT_TRUE(dbs->MigrateVersion74To75());
  ASSERT_EQ(75, dbs->GetVersion());
  dbs->EndLoad();
  ASSERT_FALSE(dbs->needs_column_refresh_);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));

  ASSERT_TRUE(connection.DoesColumnExist("models", "progress_marker"));
  ASSERT_FALSE(connection.DoesColumnExist("models",
      "last_download_timestamp"));
}

TEST_P(MigrationTest, ToCurrentVersion) {
  switch (GetParam()) {
    case 67:
      SetUpVersion67Database();
      break;
    case 68:
      SetUpVersion68Database();
      break;
    case 69:
      SetUpVersion69Database();
      break;
    case 70:
      SetUpVersion70Database();
      break;
    case 71:
      SetUpVersion71Database();
      break;
    case 72:
      SetUpVersion72Database();
      break;
    case 73:
      SetUpVersion73Database();
      break;
    case 74:
      SetUpVersion74Database();
      break;
    default:
      // If you see this error, it may mean that you've increased the
      // database version number but you haven't finished adding unit tests
      // for the database migration code.  You need to need to supply a
      // SetUpVersionXXDatabase function with a dump of the test database
      // at the old schema.  Here's one way to do that:
      //   1. Start on a clean tree (with none of your pending schema changes).
      //   2. Set a breakpoint in this function and run the unit test.
      //   3. Allow this test to run to completion (step out of the call),
      //      without allowing ~MigrationTest to execute.
      //   4. Examine this->scoped_dir_ to determine the location of the
      //      test database (it is currently of the version you need).
      //   5. Dump this using the sqlite command line tool:
      //        > .output foo_dump.sql
      //        > .dump
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

    // Renamed a column in Version 70
    ASSERT_FALSE(connection.DoesColumnExist("metas", "singleton_tag"));
    ASSERT_TRUE(connection.DoesColumnExist("metas", "unique_server_tag"));
    ASSERT_TRUE(connection.DoesColumnExist("metas", "unique_client_tag"));

    // Removed extended attributes in Version 72.
    ASSERT_FALSE(connection.DoesTableExist("extended_attributes"));

    // Columns added in Version 73.
    ASSERT_TRUE(connection.DoesColumnExist(
        "share_info", "notification_state"));

    // Columns added in version 74.
    ASSERT_TRUE(connection.DoesColumnExist("share_info",
        "autofill_migration_state"));

    // Column replaced in version 75.
    ASSERT_TRUE(connection.DoesColumnExist("models", "progress_marker"));
    ASSERT_FALSE(connection.DoesColumnExist("models",
        "last_download_timestamp"));
  }
  {
    syncable::Directory::KernelLoadInfo dir_info;
    dbs->LoadInfo(&dir_info);

    // Check download_progress state (v75 migration)
    ASSERT_EQ(694,
        dir_info.kernel_info.download_progress[syncable::BOOKMARKS]
        .timestamp_token_for_migration());
    ASSERT_FALSE(
        dir_info.kernel_info.download_progress[syncable::BOOKMARKS]
        .has_token());
    ASSERT_EQ(32904,
        dir_info.kernel_info.download_progress[syncable::BOOKMARKS]
        .data_type_id());
    ASSERT_FALSE(
        dir_info.kernel_info.download_progress[syncable::THEMES]
        .has_timestamp_token_for_migration());
    ASSERT_TRUE(
        dir_info.kernel_info.download_progress[syncable::THEMES]
        .has_token());
    ASSERT_TRUE(
        dir_info.kernel_info.download_progress[syncable::THEMES]
        .token().empty());
    ASSERT_EQ(41210,
        dir_info.kernel_info.download_progress[syncable::THEMES]
        .data_type_id());
  }

  MetahandlesIndex index;
  STLElementDeleter<MetahandlesIndex> index_deleter(&index);
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
  EXPECT_EQ("", (*it)->ref(UNIQUE_SERVER_TAG));
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
  EXPECT_EQ("google_chrome", (*it)->ref(UNIQUE_SERVER_TAG));
  EXPECT_FALSE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_FALSE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(8, (*it)->ref(META_HANDLE));
  EXPECT_EQ("google_chrome_bookmarks", (*it)->ref(UNIQUE_SERVER_TAG));
  EXPECT_TRUE((*it)->ref(SPECIFICS).HasExtension(sync_pb::bookmark));
  EXPECT_TRUE((*it)->ref(SERVER_SPECIFICS).HasExtension(sync_pb::bookmark));

  ASSERT_TRUE(++it != index.end());
  ASSERT_EQ(9, (*it)->ref(META_HANDLE));
  EXPECT_EQ("bookmark_bar", (*it)->ref(UNIQUE_SERVER_TAG));
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
  EXPECT_EQ("other_bookmarks", (*it)->ref(UNIQUE_SERVER_TAG));
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
  EXPECT_EQ("", (*it)->ref(UNIQUE_SERVER_TAG));
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
}

INSTANTIATE_TEST_CASE_P(DirectoryBackingStore, MigrationTest,
                        testing::Range(67, kCurrentDBVersion));

TEST_F(DirectoryBackingStoreTest, ModelTypeIds) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    std::string model_id =
        DirectoryBackingStore::ModelTypeEnumToModelId(ModelTypeFromInt(i));
    EXPECT_EQ(i,
        DirectoryBackingStore::ModelIdToModelTypeEnum(model_id.data(),
                                                      model_id.size()));
  }
}

TEST_F(DirectoryBackingStoreTest, Corruption) {
  {
    scoped_ptr<DirectoryBackingStore> dbs(
        new DirectoryBackingStore(GetUsername(), GetDatabasePath()));
    EXPECT_TRUE(dbs->BeginLoad());
  }
  std::string bad_data("BAD DATA");
  EXPECT_TRUE(file_util::WriteFile(GetDatabasePath(), bad_data.data(),
                                   bad_data.size()));
  {
    scoped_ptr<DirectoryBackingStore> dbs(
        new DirectoryBackingStore(GetUsername(), GetDatabasePath()));

    // In release mode, we expect the sync database to nuke itself and start
    // over if it detects invalid/corrupted data.
#if defined(NDEBUG)
    EXPECT_TRUE(dbs->BeginLoad());
#else
    EXPECT_DEATH(dbs->BeginLoad(), "sqlite error");
#endif
  }
}

TEST_F(DirectoryBackingStoreTest, DeleteEntries) {
  SetUpCurrentDatabaseAndCheckVersion();
  scoped_ptr<DirectoryBackingStore> dbs(
      new DirectoryBackingStore(GetUsername(), GetDatabasePath()));
  dbs->BeginLoad();
  MetahandlesIndex index;
  STLElementDeleter<MetahandlesIndex> index_deleter(&index);
  dbs->LoadEntries(&index);
  size_t initial_size = index.size();
  ASSERT_LT(0U, initial_size) << "Test requires entries to delete.";
  int64 first_to_die = (*index.begin())->ref(META_HANDLE);
  MetahandleSet to_delete;
  to_delete.insert(first_to_die);
  EXPECT_TRUE(dbs->DeleteEntries(to_delete));

  STLDeleteElements(&index);
  dbs->LoadEntries(&index);

  EXPECT_EQ(initial_size - 1, index.size());
  bool delete_failed = false;
  for (MetahandlesIndex::iterator it = index.begin(); it != index.end();
       ++it) {
    if ((*it)->ref(META_HANDLE) == first_to_die) {
      delete_failed = true;
      break;
    }
  }
  EXPECT_FALSE(delete_failed);

  to_delete.clear();
  for (MetahandlesIndex::iterator it = index.begin(); it != index.end();
       ++it) {
    to_delete.insert((*it)->ref(META_HANDLE));
  }

  EXPECT_TRUE(dbs->DeleteEntries(to_delete));

  STLDeleteElements(&index);
  dbs->LoadEntries(&index);
  EXPECT_EQ(0U, index.size());

  dbs->EndLoad();
  dbs->EndSave();
}

}  // namespace syncable
