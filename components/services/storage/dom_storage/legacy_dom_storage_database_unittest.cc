// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/legacy_dom_storage_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace storage {

void CreateV2Table(sql::Database* db) {
  ASSERT_TRUE(db->is_open());
  ASSERT_TRUE(db->Execute("DROP TABLE IF EXISTS ItemTable"));
  ASSERT_TRUE(
      db->Execute("CREATE TABLE ItemTable ("
                  "key TEXT UNIQUE ON CONFLICT REPLACE, "
                  "value BLOB NOT NULL ON CONFLICT FAIL)"));
}

void CreateInvalidTable(sql::Database* db) {
  // Create a table with out a key column - this is "invalid"
  // as far as the DOM Storage db is concerned.
  ASSERT_TRUE(db->is_open());
  ASSERT_TRUE(db->Execute("DROP TABLE IF EXISTS ItemTable"));
  ASSERT_TRUE(
      db->Execute("CREATE TABLE IF NOT EXISTS ItemTable ("
                  "value BLOB NOT NULL ON CONFLICT FAIL)"));
}

void CheckValuesMatch(LegacyDomStorageDatabase* db,
                      const LegacyDomStorageValuesMap& expected) {
  LegacyDomStorageValuesMap values_read;
  db->ReadAllValues(&values_read);
  EXPECT_EQ(expected.size(), values_read.size());

  LegacyDomStorageValuesMap::const_iterator it = values_read.begin();
  for (; it != values_read.end(); ++it) {
    base::string16 key = it->first;
    base::NullableString16 value = it->second;
    base::NullableString16 expected_value = expected.find(key)->second;
    EXPECT_EQ(expected_value.string(), value.string());
    EXPECT_EQ(expected_value.is_null(), value.is_null());
  }
}

void CreateMapWithValues(LegacyDomStorageValuesMap* values) {
  base::string16 kCannedKeys[] = {ASCIIToUTF16("test"), ASCIIToUTF16("company"),
                                  ASCIIToUTF16("date"), ASCIIToUTF16("empty")};
  base::NullableString16 kCannedValues[] = {
      base::NullableString16(ASCIIToUTF16("123"), false),
      base::NullableString16(ASCIIToUTF16("Google"), false),
      base::NullableString16(ASCIIToUTF16("18-01-2012"), false),
      base::NullableString16(base::string16(), false)};
  for (unsigned i = 0; i < sizeof(kCannedKeys) / sizeof(kCannedKeys[0]); i++)
    (*values)[kCannedKeys[i]] = kCannedValues[i];
}

TEST(LegacyDomStorageDatabaseTest, SimpleOpenAndClose) {
  LegacyDomStorageDatabase db;
  EXPECT_FALSE(db.IsOpen());
  ASSERT_TRUE(db.LazyOpen(true));
  EXPECT_TRUE(db.IsOpen());
  EXPECT_EQ(LegacyDomStorageDatabase::V2, db.DetectSchemaVersion());
  db.Close();
  EXPECT_FALSE(db.IsOpen());
}

TEST(LegacyDomStorageDatabaseTest, CloseEmptyDatabaseDeletesFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_name =
      temp_dir.GetPath().AppendASCII("TestLegacyDomStorageDatabase.db");
  LegacyDomStorageValuesMap storage;
  CreateMapWithValues(&storage);

  // First test the case that explicitly clearing the database will
  // trigger its deletion from disk.
  {
    LegacyDomStorageDatabase db(file_name);
    EXPECT_EQ(file_name, db.file_path());
    ASSERT_TRUE(db.CommitChanges(false, storage));
  }
  EXPECT_TRUE(base::PathExists(file_name));

  {
    // Check that reading an existing db with data in it
    // keeps the DB on disk on close.
    LegacyDomStorageDatabase db(file_name);
    LegacyDomStorageValuesMap values;
    db.ReadAllValues(&values);
    EXPECT_EQ(storage.size(), values.size());
  }

  EXPECT_TRUE(base::PathExists(file_name));
  storage.clear();

  {
    LegacyDomStorageDatabase db(file_name);
    ASSERT_TRUE(db.CommitChanges(true, storage));
  }
  EXPECT_FALSE(base::PathExists(file_name));

  // Now ensure that a series of updates and removals whose net effect
  // is an empty database also triggers deletion.
  CreateMapWithValues(&storage);
  {
    LegacyDomStorageDatabase db(file_name);
    ASSERT_TRUE(db.CommitChanges(false, storage));
  }

  EXPECT_TRUE(base::PathExists(file_name));

  {
    LegacyDomStorageDatabase db(file_name);
    ASSERT_TRUE(db.CommitChanges(false, storage));
    auto it = storage.begin();
    for (; it != storage.end(); ++it)
      it->second = base::NullableString16();
    ASSERT_TRUE(db.CommitChanges(false, storage));
  }
  EXPECT_FALSE(base::PathExists(file_name));
}

TEST(LegacyDomStorageDatabaseTest, TestLazyOpenIsLazy) {
  // This test needs to operate with a file on disk to ensure that we will
  // open a file that already exists when only invoking ReadAllValues.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_name =
      temp_dir.GetPath().AppendASCII("TestLegacyDomStorageDatabase.db");

  LegacyDomStorageDatabase db(file_name);
  EXPECT_FALSE(db.IsOpen());
  LegacyDomStorageValuesMap values;
  db.ReadAllValues(&values);
  // Reading an empty db should not open the database.
  EXPECT_FALSE(db.IsOpen());

  values[ASCIIToUTF16("key")] =
      base::NullableString16(ASCIIToUTF16("value"), false);
  db.CommitChanges(false, values);
  // Writing content should open the database.
  EXPECT_TRUE(db.IsOpen());

  db.Close();
  ASSERT_FALSE(db.IsOpen());

  // Reading from an existing database should open the database.
  CheckValuesMatch(&db, values);
  EXPECT_TRUE(db.IsOpen());
}

TEST(LegacyDomStorageDatabaseTest, TestDetectSchemaVersion) {
  LegacyDomStorageDatabase db;
  db.db_.reset(new sql::Database());
  ASSERT_TRUE(db.db_->OpenInMemory());

  CreateInvalidTable(db.db_.get());
  EXPECT_EQ(LegacyDomStorageDatabase::INVALID, db.DetectSchemaVersion());

  CreateV2Table(db.db_.get());
  EXPECT_EQ(LegacyDomStorageDatabase::V2, db.DetectSchemaVersion());
}

TEST(LegacyDomStorageDatabaseTest, SimpleWriteAndReadBack) {
  LegacyDomStorageDatabase db;

  LegacyDomStorageValuesMap storage;
  CreateMapWithValues(&storage);

  EXPECT_TRUE(db.CommitChanges(false, storage));
  CheckValuesMatch(&db, storage);
}

TEST(LegacyDomStorageDatabaseTest, WriteWithClear) {
  LegacyDomStorageDatabase db;

  LegacyDomStorageValuesMap storage;
  CreateMapWithValues(&storage);

  ASSERT_TRUE(db.CommitChanges(false, storage));
  CheckValuesMatch(&db, storage);

  // Insert some values, clearing the database first.
  storage.clear();
  storage[ASCIIToUTF16("another_key")] =
      base::NullableString16(ASCIIToUTF16("test"), false);
  ASSERT_TRUE(db.CommitChanges(true, storage));
  CheckValuesMatch(&db, storage);

  // Now clear the values without inserting any new ones.
  storage.clear();
  ASSERT_TRUE(db.CommitChanges(true, storage));
  CheckValuesMatch(&db, storage);
}

TEST(LegacyDomStorageDatabaseTest, TestSimpleRemoveOneValue) {
  LegacyDomStorageDatabase db;

  ASSERT_TRUE(db.LazyOpen(true));
  const base::string16 kCannedKey = ASCIIToUTF16("test");
  const base::NullableString16 kCannedValue(ASCIIToUTF16("data"), false);
  LegacyDomStorageValuesMap expected;
  expected[kCannedKey] = kCannedValue;

  // First write some data into the database.
  ASSERT_TRUE(db.CommitChanges(false, expected));
  CheckValuesMatch(&db, expected);

  LegacyDomStorageValuesMap values;
  // A null string in the map should mean that that key gets
  // removed.
  values[kCannedKey] = base::NullableString16();
  EXPECT_TRUE(db.CommitChanges(false, values));

  expected.clear();
  CheckValuesMatch(&db, expected);
}

TEST(LegacyDomStorageDatabaseTest, TestCanOpenAndReadWebCoreDatabase) {
  base::FilePath dir_test_data;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir_test_data));
  base::FilePath test_data = dir_test_data.AppendASCII("components")
                                 .AppendASCII("services")
                                 .AppendASCII("storage")
                                 .AppendASCII("test_data");
  test_data = test_data.AppendASCII("legacy_dom_storage_database.localstorage");
  ASSERT_TRUE(base::PathExists(test_data));

  // Create a temporary copy of the WebCore test database, so as to avoid
  // modifying the source file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath webcore_database =
      temp_dir.GetPath().AppendASCII("dom_storage");
  ASSERT_TRUE(base::CopyFile(test_data, webcore_database));

  LegacyDomStorageDatabase db(webcore_database);
  LegacyDomStorageValuesMap values;
  db.ReadAllValues(&values);
  EXPECT_TRUE(db.IsOpen());
  EXPECT_EQ(2u, values.size());

  LegacyDomStorageValuesMap::const_iterator it =
      values.find(ASCIIToUTF16("value"));
  EXPECT_TRUE(it != values.end());
  EXPECT_EQ(ASCIIToUTF16("I am in local storage!"), it->second.string());

  it = values.find(ASCIIToUTF16("timestamp"));
  EXPECT_TRUE(it != values.end());
  EXPECT_EQ(ASCIIToUTF16("1326738338841"), it->second.string());

  it = values.find(ASCIIToUTF16("not_there"));
  EXPECT_TRUE(it == values.end());
}

TEST(LegacyDomStorageDatabaseTest, TestCanOpenFileThatIsNotADatabase) {
  // Write into the temporary file first.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_name =
      temp_dir.GetPath().AppendASCII("TestLegacyDomStorageDatabase.db");

  const char kData[] = "I am not a database.";
  base::WriteFile(file_name, kData, strlen(kData));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_NOTADB);

    // Try and open the file. As it's not a database, we should end up deleting
    // it and creating a new, valid file, so everything should actually
    // succeed.
    LegacyDomStorageDatabase db(file_name);
    LegacyDomStorageValuesMap values;
    CreateMapWithValues(&values);
    EXPECT_TRUE(db.CommitChanges(true, values));
    EXPECT_TRUE(db.CommitChanges(false, values));
    EXPECT_TRUE(db.IsOpen());

    CheckValuesMatch(&db, values);

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CANTOPEN);

    // Try to open a directory, we should fail gracefully and not attempt
    // to delete it.
    LegacyDomStorageDatabase db(temp_dir.GetPath());
    LegacyDomStorageValuesMap values;
    CreateMapWithValues(&values);
    EXPECT_FALSE(db.CommitChanges(true, values));
    EXPECT_FALSE(db.CommitChanges(false, values));
    EXPECT_FALSE(db.IsOpen());

    values.clear();

    db.ReadAllValues(&values);
    EXPECT_EQ(0u, values.size());
    EXPECT_FALSE(db.IsOpen());

    EXPECT_TRUE(base::PathExists(temp_dir.GetPath()));

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }
}

}  // namespace storage
