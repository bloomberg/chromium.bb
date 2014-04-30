// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_database.pb.h"
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

void VerifyRegistrationData(
    const ServiceWorkerDatabase::RegistrationData& expected,
    const ServiceWorkerDatabase::RegistrationData& actual) {
  EXPECT_EQ(expected.registration_id, actual.registration_id);
  EXPECT_EQ(expected.scope, actual.scope);
  EXPECT_EQ(expected.script, actual.script);
  EXPECT_EQ(expected.version_id, actual.version_id);
  EXPECT_EQ(expected.is_active, actual.is_active);
  EXPECT_EQ(expected.has_fetch_handler, actual.has_fetch_handler);
  EXPECT_EQ(expected.last_update_check, actual.last_update_check);
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
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());
  scoped_ptr<ServiceWorkerDatabase> database(
      CreateDatabase(database_dir.path()));
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

  // Writing a registration bumps the next available ids.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  ServiceWorkerDatabase::RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = GURL("http://example.com/foo");
  data1.script = GURL("http://example.com/script1.js");
  data1.version_id = 200;
  ASSERT_TRUE(database->WriteRegistration(data1, resources));

  EXPECT_TRUE(database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(101, ids.reg_id);
  EXPECT_EQ(201, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);

  // Writing a registration whose ids are lower than the stored ones should not
  // bump the next available ids.
  ServiceWorkerDatabase::RegistrationData data2;
  data2.registration_id = 10;
  data2.scope = GURL("http://example.com/bar");
  data2.script = GURL("http://example.com/script2.js");
  data2.version_id = 20;
  ASSERT_TRUE(database->WriteRegistration(data2, resources));

  // Close and reopen the database to verify the stored values.
  database.reset(CreateDatabase(database_dir.path()));

  EXPECT_TRUE(database->GetNextAvailableIds(
      &ids.reg_id, &ids.ver_id, &ids.res_id));
  EXPECT_EQ(101, ids.reg_id);
  EXPECT_EQ(201, ids.ver_id);
  EXPECT_EQ(0, ids.res_id);
}

TEST(ServiceWorkerDatabaseTest, GetOriginsWithRegistrations) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  std::set<GURL> origins;
  EXPECT_FALSE(database->GetOriginsWithRegistrations(&origins));
  EXPECT_TRUE(origins.empty());

  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;

  ServiceWorkerDatabase::RegistrationData data1;
  data1.registration_id = 123;
  data1.scope = GURL("http://example.com/foo");
  data1.script = GURL("http://example.com/script1.js");
  data1.version_id = 456;
  GURL origin1 = data1.script.GetOrigin();
  ASSERT_TRUE(database->WriteRegistration(data1, resources));

  ServiceWorkerDatabase::RegistrationData data2;
  data2.registration_id = 234;
  data2.scope = GURL("https://www.example.com/bar");
  data2.script = GURL("https://www.example.com/script2.js");
  data2.version_id = 567;
  GURL origin2 = data2.script.GetOrigin();
  ASSERT_TRUE(database->WriteRegistration(data2, resources));

  ServiceWorkerDatabase::RegistrationData data3;
  data3.registration_id = 345;
  data3.scope = GURL("https://example.org/hoge");
  data3.script = GURL("https://example.org/script3.js");
  data3.version_id = 678;
  GURL origin3 = data3.script.GetOrigin();
  ASSERT_TRUE(database->WriteRegistration(data3, resources));

  // |origin3| has two registrations.
  ServiceWorkerDatabase::RegistrationData data4;
  data4.registration_id = 456;
  data4.scope = GURL("https://example.org/fuga");
  data4.script = GURL("https://example.org/script4.js");
  data4.version_id = 789;
  ASSERT_EQ(origin3, data4.scope.GetOrigin());
  ASSERT_TRUE(database->WriteRegistration(data4, resources));

  origins.clear();
  EXPECT_TRUE(database->GetOriginsWithRegistrations(&origins));
  EXPECT_EQ(3U, origins.size());
  EXPECT_TRUE(ContainsKey(origins, origin1));
  EXPECT_TRUE(ContainsKey(origins, origin2));
  EXPECT_TRUE(ContainsKey(origins, origin3));

  // |origin3| has another registration, so should not remove it from the
  // unique origin list.
  ASSERT_TRUE(database->DeleteRegistration(data4.registration_id, origin3));

  origins.clear();
  EXPECT_TRUE(database->GetOriginsWithRegistrations(&origins));
  EXPECT_EQ(3U, origins.size());
  EXPECT_TRUE(ContainsKey(origins, origin1));
  EXPECT_TRUE(ContainsKey(origins, origin2));
  EXPECT_TRUE(ContainsKey(origins, origin3));

  // |origin3| should be removed from the unique origin list.
  ASSERT_TRUE(database->DeleteRegistration(data3.registration_id, origin3));

  origins.clear();
  EXPECT_TRUE(database->GetOriginsWithRegistrations(&origins));
  EXPECT_EQ(2U, origins.size());
  EXPECT_TRUE(ContainsKey(origins, origin1));
  EXPECT_TRUE(ContainsKey(origins, origin2));
}

TEST(ServiceWorkerDatabaseTest, GetRegistrationsForOrigin) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  GURL origin("https://example.org");
  std::vector<ServiceWorkerDatabase::RegistrationData> registrations;
  EXPECT_FALSE(database->GetRegistrationsForOrigin(origin, &registrations));

  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;

  ServiceWorkerDatabase::RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = GURL("http://example.com/foo");
  data1.script = GURL("http://example.com/script1.js");
  data1.version_id = 1000;
  ASSERT_TRUE(database->WriteRegistration(data1, resources));

  ServiceWorkerDatabase::RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = GURL("https://www.example.com/bar");
  data2.script = GURL("https://www.example.com/script2.js");
  data2.version_id = 2000;
  ASSERT_TRUE(database->WriteRegistration(data2, resources));

  ServiceWorkerDatabase::RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = GURL("https://example.org/hoge");
  data3.script = GURL("https://example.org/script3.js");
  data3.version_id = 3000;
  ASSERT_TRUE(database->WriteRegistration(data3, resources));

  // Same origin with |data3|.
  ServiceWorkerDatabase::RegistrationData data4;
  data4.registration_id = 400;
  data4.scope = GURL("https://example.org/fuga");
  data4.script = GURL("https://example.org/script4.js");
  data4.version_id = 4000;
  ASSERT_TRUE(database->WriteRegistration(data4, resources));

  EXPECT_TRUE(database->GetRegistrationsForOrigin(origin, &registrations));
  EXPECT_EQ(2U, registrations.size());
  VerifyRegistrationData(data3, registrations[0]);
  VerifyRegistrationData(data4, registrations[1]);
}

TEST(ServiceWorkerDatabaseTest, Registration) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = 100;
  data.scope = GURL("http://example.com/foo");
  data.script = GURL("http://example.com/script.js");
  data.version_id = 200;
  GURL origin = data.scope.GetOrigin();

  // TODO(nhiroki): Test ResourceRecord manipulation.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;

  EXPECT_TRUE(database->WriteRegistration(data, resources));

  ServiceWorkerDatabase::RegistrationData data_out;
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources_out;
  EXPECT_TRUE(database->ReadRegistration(
      data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);

  EXPECT_TRUE(database->DeleteRegistration(data.registration_id,
                                           data.scope.GetOrigin()));

  EXPECT_FALSE(database->ReadRegistration(
      data.registration_id, origin, &data_out, &resources_out));
}

TEST(ServiceWorkerDatabaseTest, UncommittedResourceIds) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Write {1, 2, 3}.
  std::set<int64> ids1;
  ids1.insert(1);
  ids1.insert(2);
  ids1.insert(3);
  EXPECT_TRUE(database->WriteUncommittedResourceIds(ids1));

  std::set<int64> ids_out;
  EXPECT_TRUE(database->GetUncommittedResourceIds(&ids_out));
  EXPECT_EQ(ids1.size(), ids_out.size());
  EXPECT_TRUE(base::STLIncludes(ids1, ids_out));

  // Write {2, 4}.
  std::set<int64> ids2;
  ids2.insert(2);
  ids2.insert(4);
  EXPECT_TRUE(database->WriteUncommittedResourceIds(ids2));

  ids_out.clear();
  EXPECT_TRUE(database->GetUncommittedResourceIds(&ids_out));
  EXPECT_EQ(4U, ids_out.size());
  EXPECT_TRUE(ContainsKey(ids_out, 1));
  EXPECT_TRUE(ContainsKey(ids_out, 2));
  EXPECT_TRUE(ContainsKey(ids_out, 3));
  EXPECT_TRUE(ContainsKey(ids_out, 4));

  // Delete {2, 3}.
  std::set<int64> ids3;
  ids3.insert(2);
  ids3.insert(3);
  EXPECT_TRUE(database->ClearUncommittedResourceIds(ids3));

  ids_out.clear();
  EXPECT_TRUE(database->GetUncommittedResourceIds(&ids_out));
  EXPECT_EQ(2U, ids_out.size());
  EXPECT_TRUE(ContainsKey(ids_out, 1));
  EXPECT_TRUE(ContainsKey(ids_out, 4));
}

TEST(ServiceWorkerDatabaseTest, PurgeableResourceIds) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Write {1, 2, 3}.
  std::set<int64> ids1;
  ids1.insert(1);
  ids1.insert(2);
  ids1.insert(3);
  EXPECT_TRUE(database->WritePurgeableResourceIds(ids1));

  std::set<int64> ids_out;
  EXPECT_TRUE(database->GetPurgeableResourceIds(&ids_out));
  EXPECT_EQ(ids1.size(), ids_out.size());
  EXPECT_TRUE(base::STLIncludes(ids1, ids_out));

  // Write {2, 4}.
  std::set<int64> ids2;
  ids2.insert(2);
  ids2.insert(4);
  EXPECT_TRUE(database->WritePurgeableResourceIds(ids2));

  ids_out.clear();
  EXPECT_TRUE(database->GetPurgeableResourceIds(&ids_out));
  EXPECT_EQ(4U, ids_out.size());
  EXPECT_TRUE(ContainsKey(ids_out, 1));
  EXPECT_TRUE(ContainsKey(ids_out, 2));
  EXPECT_TRUE(ContainsKey(ids_out, 3));
  EXPECT_TRUE(ContainsKey(ids_out, 4));

  // Delete {2, 3}.
  std::set<int64> ids3;
  ids3.insert(2);
  ids3.insert(3);
  EXPECT_TRUE(database->ClearPurgeableResourceIds(ids3));

  ids_out.clear();
  EXPECT_TRUE(database->GetPurgeableResourceIds(&ids_out));
  EXPECT_EQ(2U, ids_out.size());
  EXPECT_TRUE(ContainsKey(ids_out, 1));
  EXPECT_TRUE(ContainsKey(ids_out, 4));
}

}  // namespace content
