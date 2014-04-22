// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

struct AvailableIds {
  int64 reg_id;
  int64 res_id;
  int64 ver_id;

  AvailableIds() : reg_id(-1), res_id(-1), ver_id(-1) {}
  ~AvailableIds() {}
};

ServiceWorkerDatabase* CreateDatabase(const base::FilePath& path) {
  return new ServiceWorkerDatabase(path);
}

ServiceWorkerDatabase* CreateDatabaseInMemory() {
  return new ServiceWorkerDatabase(base::FilePath());
}

}  // namespace

TEST(ServiceWorkerDatabaseTest, OpenDatabase) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  scoped_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.path()));

  // Should be false because the database does not exist at the path.
  EXPECT_FALSE(database->LazyOpen(false));

  EXPECT_TRUE(database->LazyOpen(true));

  database.reset(CreateDatabase(database_dir.path()));
  EXPECT_TRUE(database->LazyOpen(false));
}

TEST(ServiceWorkerDatabaseTest, OpenDatabase_InMemory) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Should be false because the database does not exist in memory.
  EXPECT_FALSE(database->LazyOpen(false));

  EXPECT_TRUE(database->LazyOpen(true));
  database.reset(CreateDatabaseInMemory());

  // Should be false because the database is not persistent.
  EXPECT_FALSE(database->LazyOpen(false));
}

TEST(ServiceWorkerDatabaseTest, GetNextAvailableIds) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  AvailableIds ids;

  // Should be false because the database hasn't been opened.
  EXPECT_FALSE(database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));

  ASSERT_TRUE(database->LazyOpen(true));
  EXPECT_TRUE(database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(0, ids.reg_id);
  EXPECT_EQ(0, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);

  // TODO(nhiroki): Test GetNextAvailableIds() after update these ids.
}

}  // namespace content
