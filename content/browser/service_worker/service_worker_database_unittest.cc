// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_database.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/service_worker_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

typedef ServiceWorkerDatabase::RegistrationData RegistrationData;
typedef ServiceWorkerDatabase::ResourceRecord Resource;

struct AvailableIds {
  int64 reg_id;
  int64 res_id;
  int64 ver_id;

  AvailableIds() : reg_id(-1), res_id(-1), ver_id(-1) {}
  ~AvailableIds() {}
};

// TODO(nhiroki): Refactor tests using this helper.
GURL URL(const GURL& origin, const std::string& path) {
  EXPECT_TRUE(origin.is_valid());
  GURL out(origin.GetOrigin().spec() + path);
  EXPECT_TRUE(out.is_valid());
  return out;
}

// TODO(nhiroki): Remove this.
Resource CreateResource(int64 resource_id, const std::string& url) {
  Resource resource;
  resource.resource_id = resource_id;
  resource.url = GURL(url);
  EXPECT_TRUE(resource.url.is_valid());
  return resource;
}

Resource CreateResource(int64 resource_id, const GURL& url) {
  Resource resource;
  resource.resource_id = resource_id;
  resource.url = url;
  EXPECT_TRUE(resource.url.is_valid());
  return resource;
}

ServiceWorkerDatabase* CreateDatabase(const base::FilePath& path) {
  return new ServiceWorkerDatabase(path);
}

ServiceWorkerDatabase* CreateDatabaseInMemory() {
  return new ServiceWorkerDatabase(base::FilePath());
}

void VerifyRegistrationData(const RegistrationData& expected,
                            const RegistrationData& actual) {
  EXPECT_EQ(expected.registration_id, actual.registration_id);
  EXPECT_EQ(expected.scope, actual.scope);
  EXPECT_EQ(expected.script, actual.script);
  EXPECT_EQ(expected.version_id, actual.version_id);
  EXPECT_EQ(expected.is_active, actual.is_active);
  EXPECT_EQ(expected.has_fetch_handler, actual.has_fetch_handler);
  EXPECT_EQ(expected.last_update_check, actual.last_update_check);
}

void VerifyResourceRecords(const std::vector<Resource>& expected,
                           const std::vector<Resource>& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  if (expected.size() != actual.size())
    return;
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i].resource_id, actual[i].resource_id);
    EXPECT_EQ(expected[i].url, actual[i].url);
  }
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

TEST(ServiceWorkerDatabaseTest, DatabaseVersion) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());
  EXPECT_TRUE(database->LazyOpen(true));

  // Opening a new database does not write anything, so its schema version
  // should be 0.
  int64 db_version = -1;
  EXPECT_TRUE(database->ReadDatabaseVersion(&db_version));
  EXPECT_EQ(0u, db_version);

  // First writing triggers database initialization and bumps the schema
  // version.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  ServiceWorkerDatabase::RegistrationData data;
  ASSERT_TRUE(database->WriteRegistration(data, resources));

  EXPECT_TRUE(database->ReadDatabaseVersion(&db_version));
  EXPECT_LT(0, db_version);
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
  std::vector<Resource> resources;
  RegistrationData data1;
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
  RegistrationData data2;
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

  std::vector<Resource> resources;

  RegistrationData data1;
  data1.registration_id = 123;
  data1.scope = GURL("http://example.com/foo");
  data1.script = GURL("http://example.com/script1.js");
  data1.version_id = 456;
  GURL origin1 = data1.script.GetOrigin();
  ASSERT_TRUE(database->WriteRegistration(data1, resources));

  RegistrationData data2;
  data2.registration_id = 234;
  data2.scope = GURL("https://www.example.com/bar");
  data2.script = GURL("https://www.example.com/script2.js");
  data2.version_id = 567;
  GURL origin2 = data2.script.GetOrigin();
  ASSERT_TRUE(database->WriteRegistration(data2, resources));

  RegistrationData data3;
  data3.registration_id = 345;
  data3.scope = GURL("https://example.org/hoge");
  data3.script = GURL("https://example.org/script3.js");
  data3.version_id = 678;
  GURL origin3 = data3.script.GetOrigin();
  ASSERT_TRUE(database->WriteRegistration(data3, resources));

  // |origin3| has two registrations.
  RegistrationData data4;
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
  std::vector<RegistrationData> registrations;
  EXPECT_FALSE(database->GetRegistrationsForOrigin(origin, &registrations));

  std::vector<Resource> resources;

  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = GURL("http://example.com/foo");
  data1.script = GURL("http://example.com/script1.js");
  data1.version_id = 1000;
  ASSERT_TRUE(database->WriteRegistration(data1, resources));

  RegistrationData data2;
  data2.registration_id = 200;
  data2.scope = GURL("https://www.example.com/bar");
  data2.script = GURL("https://www.example.com/script2.js");
  data2.version_id = 2000;
  ASSERT_TRUE(database->WriteRegistration(data2, resources));

  RegistrationData data3;
  data3.registration_id = 300;
  data3.scope = GURL("https://example.org/hoge");
  data3.script = GURL("https://example.org/script3.js");
  data3.version_id = 3000;
  ASSERT_TRUE(database->WriteRegistration(data3, resources));

  // Same origin with |data3|.
  RegistrationData data4;
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

// TODO(nhiroki): Record read/write operations using gtest fixture to avoid
// building expectations by hand. For example, expected purgeable resource ids
// should be calculated automatically.
TEST(ServiceWorkerDatabaseTest, Registration_Basic) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  RegistrationData data;
  data.registration_id = 100;
  data.scope = GURL("http://example.com/foo");
  data.script = GURL("http://example.com/script.js");
  data.version_id = 200;
  GURL origin = data.scope.GetOrigin();

  Resource resource1 = CreateResource(1, "http://example.com/resource1");
  Resource resource2 = CreateResource(2, "http://example.com/resource2");

  std::vector<Resource> resources;
  resources.push_back(resource1);
  resources.push_back(resource2);

  // Write |resource1| to the uncommitted list to make sure that writing
  // registration removes resource ids associated with the registration from
  // the uncommitted list.
  std::set<int64> uncommitted_ids;
  uncommitted_ids.insert(resource1.resource_id);
  EXPECT_TRUE(database->WriteUncommittedResourceIds(uncommitted_ids));
  std::set<int64> uncommitted_ids_out;
  EXPECT_TRUE(database->GetUncommittedResourceIds(&uncommitted_ids_out));
  EXPECT_EQ(1u, uncommitted_ids_out.size());
  EXPECT_TRUE(ContainsKey(uncommitted_ids_out, resource1.resource_id));

  EXPECT_TRUE(database->WriteRegistration(data, resources));

  // Make sure that the registration and resource records are stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_TRUE(database->ReadRegistration(
      data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);
  VerifyResourceRecords(resources, resources_out);

  // Make sure that |resource1| is removed from the uncommitted list.
  uncommitted_ids_out.clear();
  EXPECT_TRUE(database->GetUncommittedResourceIds(&uncommitted_ids_out));
  EXPECT_TRUE(uncommitted_ids_out.empty());

  EXPECT_TRUE(database->DeleteRegistration(
      data.registration_id, data.scope.GetOrigin()));

  // Make sure that the registration and resource records are gone.
  resources_out.clear();
  EXPECT_FALSE(database->ReadRegistration(
      data.registration_id, origin, &data_out, &resources_out));
  EXPECT_TRUE(resources_out.empty());

  // Resources should be purgeable because these are no longer referred.
  std::set<int64> purgeable_resource_ids;
  EXPECT_TRUE(database->GetPurgeableResourceIds(&purgeable_resource_ids));
  EXPECT_EQ(2u, purgeable_resource_ids.size());
  EXPECT_TRUE(ContainsKey(purgeable_resource_ids, resource1.resource_id));
  EXPECT_TRUE(ContainsKey(purgeable_resource_ids, resource2.resource_id));
}

TEST(ServiceWorkerDatabaseTest, Registration_Overwrite) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  RegistrationData data;
  data.registration_id = 100;
  data.scope = GURL("http://example.com/foo");
  data.script = GURL("http://example.com/script.js");
  data.version_id = 200;
  GURL origin = data.scope.GetOrigin();

  Resource resource1 = CreateResource(1, "http://example.com/resource1");
  Resource resource2 = CreateResource(2, "http://example.com/resource2");

  std::vector<Resource> resources1;
  resources1.push_back(resource1);
  resources1.push_back(resource2);

  EXPECT_TRUE(database->WriteRegistration(data, resources1));

  // Make sure that the registration and resource records are stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_TRUE(database->ReadRegistration(
      data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(data, data_out);
  VerifyResourceRecords(resources1, resources_out);

  // Update the registration.
  RegistrationData updated_data = data;
  updated_data.version_id = data.version_id + 1;
  Resource resource3 = CreateResource(3, "http://example.com/resource3");
  Resource resource4 = CreateResource(4, "http://example.com/resource4");
  std::vector<Resource> resources2;
  resources2.push_back(resource3);
  resources2.push_back(resource4);

  EXPECT_TRUE(database->WriteRegistration(updated_data, resources2));

  // Make sure that |updated_data| is stored and resources referred from |data|
  // is moved to the purgeable list.
  resources_out.clear();
  EXPECT_TRUE(database->ReadRegistration(
      updated_data.registration_id, origin, &data_out, &resources_out));
  VerifyRegistrationData(updated_data, data_out);
  VerifyResourceRecords(resources2, resources_out);

  std::set<int64> purgeable_resource_ids;
  EXPECT_TRUE(database->GetPurgeableResourceIds(&purgeable_resource_ids));
  EXPECT_EQ(2u, purgeable_resource_ids.size());
  EXPECT_TRUE(ContainsKey(purgeable_resource_ids, resource1.resource_id));
  EXPECT_TRUE(ContainsKey(purgeable_resource_ids, resource2.resource_id));
}

TEST(ServiceWorkerDatabaseTest, Registration_Multiple) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Add registration1.
  RegistrationData data1;
  data1.registration_id = 100;
  data1.scope = GURL("http://example.com/foo");
  data1.script = GURL("http://example.com/script1.js");
  data1.version_id = 200;
  GURL origin1 = data1.scope.GetOrigin();

  Resource resource1 = CreateResource(1, "http://example.com/resource1");
  Resource resource2 = CreateResource(2, "http://example.com/resource2");

  std::vector<Resource> resources1;
  resources1.push_back(resource1);
  resources1.push_back(resource2);
  EXPECT_TRUE(database->WriteRegistration(data1, resources1));

  // Add registration2.
  RegistrationData data2;
  data2.registration_id = 101;
  data2.scope = GURL("http://example.com/bar");
  data2.script = GURL("http://example.com/script2.js");
  data2.version_id = 201;
  GURL origin2 = data2.scope.GetOrigin();

  Resource resource3 = CreateResource(3, "http://example.com/resource3");
  Resource resource4 = CreateResource(4, "http://example.com/resource4");

  std::vector<Resource> resources2;
  resources2.push_back(resource3);
  resources2.push_back(resource4);
  EXPECT_TRUE(database->WriteRegistration(data2, resources2));

  // Make sure that registration1 is stored.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_TRUE(database->ReadRegistration(
      data1.registration_id, origin1, &data_out, &resources_out));
  VerifyRegistrationData(data1, data_out);
  VerifyResourceRecords(resources1, resources_out);

  // Make sure that registration2 is also stored.
  resources_out.clear();
  EXPECT_TRUE(database->ReadRegistration(
      data2.registration_id, origin2, &data_out, &resources_out));
  VerifyRegistrationData(data2, data_out);
  VerifyResourceRecords(resources2, resources_out);

  std::set<int64> purgeable_resource_ids;
  EXPECT_TRUE(database->GetPurgeableResourceIds(&purgeable_resource_ids));
  EXPECT_TRUE(purgeable_resource_ids.empty());

  // Delete registration1.
  EXPECT_TRUE(database->DeleteRegistration(data1.registration_id, origin1));

  // Make sure that registration1 is gone.
  resources_out.clear();
  EXPECT_FALSE(database->ReadRegistration(
      data1.registration_id, origin1, &data_out, &resources_out));
  EXPECT_TRUE(resources_out.empty());

  purgeable_resource_ids.clear();
  EXPECT_TRUE(database->GetPurgeableResourceIds(&purgeable_resource_ids));
  EXPECT_EQ(2u, purgeable_resource_ids.size());
  EXPECT_TRUE(ContainsKey(purgeable_resource_ids, resource1.resource_id));
  EXPECT_TRUE(ContainsKey(purgeable_resource_ids, resource2.resource_id));

  // Make sure that registration2 is still alive.
  resources_out.clear();
  EXPECT_TRUE(database->ReadRegistration(
      data2.registration_id, origin2, &data_out, &resources_out));
  VerifyRegistrationData(data2, data_out);
  VerifyResourceRecords(resources2, resources_out);
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

TEST(ServiceWorkerDatabaseTest, DeleteAllDataForOrigin) {
  scoped_ptr<ServiceWorkerDatabase> database(CreateDatabaseInMemory());

  // Data associated with |origin1| will be removed.
  GURL origin1("http://example.com");
  GURL origin2("http://example.org");

  // |origin1| has two registrations.
  RegistrationData data1;
  data1.registration_id = 10;
  data1.scope = URL(origin1, "/foo");
  data1.script = URL(origin1, "/script1.js");
  data1.version_id = 100;

  std::vector<Resource> resources1;
  resources1.push_back(CreateResource(1, URL(origin1, "/resource1")));
  resources1.push_back(CreateResource(2, URL(origin1, "/resource2")));
  ASSERT_TRUE(database->WriteRegistration(data1, resources1));

  RegistrationData data2;
  data2.registration_id = 11;
  data2.scope = URL(origin1, "/bar");
  data2.script = URL(origin1, "/script2.js");
  data2.version_id = 101;

  std::vector<Resource> resources2;
  resources2.push_back(CreateResource(3, URL(origin1, "/resource3")));
  resources2.push_back(CreateResource(4, URL(origin1, "/resource4")));
  ASSERT_TRUE(database->WriteRegistration(data2, resources2));

  // |origin2| has one registration.
  RegistrationData data3;
  data3.registration_id = 12;
  data3.scope = URL(origin2, "/hoge");
  data3.script = URL(origin2, "/script3.js");
  data3.version_id = 102;

  std::vector<Resource> resources3;
  resources3.push_back(CreateResource(5, URL(origin2, "/resource5")));
  resources3.push_back(CreateResource(6, URL(origin2, "/resource6")));
  ASSERT_TRUE(database->WriteRegistration(data3, resources3));

  EXPECT_TRUE(database->DeleteAllDataForOrigin(origin1));

  // |origin1| should be removed from the unique origin list.
  std::set<GURL> unique_origins;
  EXPECT_TRUE(database->GetOriginsWithRegistrations(&unique_origins));
  EXPECT_EQ(1u, unique_origins.size());
  EXPECT_TRUE(ContainsKey(unique_origins, origin2));

  // The registrations for |origin1| should be removed.
  std::vector<RegistrationData> registrations;
  EXPECT_TRUE(database->GetRegistrationsForOrigin(origin1, &registrations));
  EXPECT_TRUE(registrations.empty());

  // The registration for |origin2| should not be removed.
  RegistrationData data_out;
  std::vector<Resource> resources_out;
  EXPECT_TRUE(database->ReadRegistration(
      data3.registration_id, origin2, &data_out, &resources_out));
  VerifyRegistrationData(data3, data_out);
  VerifyResourceRecords(resources3, resources_out);

  // The resources associated with |origin1| should be purgeable.
  std::set<int64> purgeable_ids_out;
  EXPECT_TRUE(database->GetPurgeableResourceIds(&purgeable_ids_out));
  EXPECT_EQ(4u, purgeable_ids_out.size());
  EXPECT_TRUE(ContainsKey(purgeable_ids_out, 1));
  EXPECT_TRUE(ContainsKey(purgeable_ids_out, 2));
  EXPECT_TRUE(ContainsKey(purgeable_ids_out, 3));
  EXPECT_TRUE(ContainsKey(purgeable_ids_out, 4));
}

}  // namespace content
