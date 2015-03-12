// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace content
