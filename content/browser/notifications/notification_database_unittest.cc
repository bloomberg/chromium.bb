// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace content {

class NotificationDatabaseTest : public ::testing::Test {
 protected:
  // Creates a new NotificationDatabase instance in memory.
  NotificationDatabase* CreateDatabaseInMemory() {
    return new NotificationDatabase(base::FilePath());
  }

  // Creates a new NotificationDatabase instance in |path|.
  NotificationDatabase* CreateDatabaseOnFileSystem(
      const base::FilePath& path) {
    return new NotificationDatabase(path);
  }

  // Returns if |database| has been opened.
  bool IsDatabaseOpen(NotificationDatabase* database) {
    return database->IsOpen();
  }

  // Returns if |database| is an in-memory only database.
  bool IsInMemoryDatabase(NotificationDatabase* database) {
    return database->IsInMemoryDatabase();
  }

  // Writes a LevelDB key-value pair directly to the LevelDB backing the
  // notification database in |database|.
  void WriteLevelDBKeyValuePair(NotificationDatabase* database,
                                const std::string& key,
                                const std::string& value) {
    leveldb::Status status =
        database->GetDBForTesting()->Put(leveldb::WriteOptions(), key, value);
    ASSERT_TRUE(status.ok());
  }

  // Increments the next notification id value in the database. Normally this
  // is managed by the NotificationDatabase when writing notification data.
  //
  // TODO(peter): Stop doing this manually when writing notification data will
  // do this for us, except for tests verifying corruption behavior.
  void IncrementNextNotificationId(NotificationDatabase* database) {
    int64_t next_notification_id;
    ASSERT_EQ(NotificationDatabase::STATUS_OK,
              database->GetNextNotificationId(&next_notification_id));

    leveldb::WriteBatch batch;
    database->WriteNextNotificationId(&batch, next_notification_id + 1);

    leveldb::Status status =
        database->GetDBForTesting()->Write(leveldb::WriteOptions(), &batch);

    ASSERT_TRUE(status.ok());
  }
};

TEST_F(NotificationDatabaseTest, OpenCloseMemory) {
  scoped_ptr<NotificationDatabase> database(CreateDatabaseInMemory());

  // Should return false because the database does not exist in memory.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));

  // Should return true, indicating that the database could be created.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  EXPECT_TRUE(IsDatabaseOpen(database.get()));
  EXPECT_TRUE(IsInMemoryDatabase(database.get()));

  // Verify that in-memory databases do not persist when being re-created.
  database.reset(CreateDatabaseInMemory());

  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, OpenCloseFileSystem) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  scoped_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  // Should return false because the database does not exist on the file system.
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));

  // Should return true, indicating that the database could be created.
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  EXPECT_TRUE(IsDatabaseOpen(database.get()));
  EXPECT_FALSE(IsInMemoryDatabase(database.get()));

  // Close the database, and re-open it without attempting to create it because
  // the files on the file system should still exist as expected.
  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, DestroyDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  scoped_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  EXPECT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));
  EXPECT_TRUE(IsDatabaseOpen(database.get()));

  // Destroy the database. This will immediately close it as well.
  ASSERT_EQ(NotificationDatabase::STATUS_OK, database->Destroy());
  EXPECT_FALSE(IsDatabaseOpen(database.get()));

  // Try to re-open the database (but not re-create it). This should fail as
  // the files associated with the database should have been blown away.
  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_NOT_FOUND,
            database->Open(false /* create_if_missing */));
}

TEST_F(NotificationDatabaseTest, GetNextNotificationIdIncrements) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  scoped_ptr<NotificationDatabase> database(
      CreateDatabaseOnFileSystem(database_dir.path()));

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  int64_t notification_id = 0;

  // Verify that getting two ids on the same database instance results in
  // incrementing values. Notification ids will start at 1.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->GetNextNotificationId(&notification_id));
  EXPECT_EQ(1, notification_id);

  ASSERT_NO_FATAL_FAILURE(IncrementNextNotificationId(database.get()));

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->GetNextNotificationId(&notification_id));
  EXPECT_EQ(2, notification_id);

  ASSERT_NO_FATAL_FAILURE(IncrementNextNotificationId(database.get()));

  database.reset(CreateDatabaseOnFileSystem(database_dir.path()));
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(false /* create_if_missing */));

  // Verify that the next notification id was stored in the database, and
  // continues where we expect it to be, even after closing and opening it.
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->GetNextNotificationId(&notification_id));
  EXPECT_EQ(3, notification_id);
}

TEST_F(NotificationDatabaseTest, GetNextNotificationIdCorruption) {
  scoped_ptr<NotificationDatabase> database(CreateDatabaseInMemory());
  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->Open(true /* create_if_missing */));

  int64_t notification_id = 0;

  ASSERT_EQ(NotificationDatabase::STATUS_OK,
            database->GetNextNotificationId(&notification_id));
  EXPECT_EQ(1, notification_id);

  // Deliberately write an invalid value as the next notification id to the
  // database, which should cause GetNextNotificationId to realize that
  // something is wrong with the data it's reading.
  ASSERT_NO_FATAL_FAILURE(WriteLevelDBKeyValuePair(database.get(),
                                                   "NEXT_NOTIFICATION_ID",
                                                   "-42"));

  EXPECT_EQ(NotificationDatabase::STATUS_ERROR_CORRUPTED,
            database->GetNextNotificationId(&notification_id));
}

}  // namespace content
