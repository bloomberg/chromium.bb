// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache_migrator.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kMaxDiskCacheSize = 250 * 1024 * 1024;

struct ResponseData {
  int64 resource_id;
  std::string headers;
  std::string body;
  std::string metadata;

  ResponseData(int64 resource_id,
               const std::string& headers,
               const std::string& body,
               const std::string& metadata)
      : resource_id(resource_id),
        headers(headers),
        body(body),
        metadata(metadata) {}
};

void OnDiskCacheMigrated(const base::Closure& callback,
                         ServiceWorkerStatusCode status) {
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  callback.Run();
}

void OnRegistrationFound(
    const base::Closure& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  callback.Run();
}

}  // namespace

class ServiceWorkerDiskCacheMigratorTest : public testing::Test {
 public:
  ServiceWorkerDiskCacheMigratorTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());
    scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager(
        new MockServiceWorkerDatabaseTaskManager(
            base::ThreadTaskRunnerHandle::Get()));

    context_.reset(new ServiceWorkerContextCore(
        user_data_directory_.path(), database_task_manager.Pass(),
        base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr, nullptr,
        nullptr));
  }

  void TearDown() override {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  scoped_ptr<ServiceWorkerDiskCache> CreateSrcDiskCache() {
#if defined(OS_ANDROID)
    // Android has already used the Simple backend.
    scoped_ptr<ServiceWorkerDiskCache> src(
        ServiceWorkerDiskCache::CreateWithSimpleBackend());
#else
    scoped_ptr<ServiceWorkerDiskCache> src(
        ServiceWorkerDiskCache::CreateWithBlockFileBackend());
#endif  // defined(OS_ANDROID)

    net::TestCompletionCallback cb;
    src->InitWithDiskBackend(
        storage()->GetOldDiskCachePath(), kMaxDiskCacheSize, false /* force */,
        base::ThreadTaskRunnerHandle::Get(), cb.callback());
    EXPECT_EQ(net::OK, cb.WaitForResult());
    return src.Pass();
  }

  scoped_ptr<ServiceWorkerDiskCache> CreateDestDiskCache() {
    scoped_ptr<ServiceWorkerDiskCache> dest(
        ServiceWorkerDiskCache::CreateWithSimpleBackend());
    net::TestCompletionCallback cb;
    dest->InitWithDiskBackend(
        storage()->GetDiskCachePath(), kMaxDiskCacheSize, false /* force */,
        base::ThreadTaskRunnerHandle::Get(), cb.callback());
    EXPECT_EQ(net::OK, cb.WaitForResult());
    return dest.Pass();
  }

  scoped_ptr<ServiceWorkerDiskCacheMigrator> CreateMigrator() {
    return make_scoped_ptr(new ServiceWorkerDiskCacheMigrator(
        storage()->GetOldDiskCachePath(), storage()->GetDiskCachePath(),
        kMaxDiskCacheSize, base::ThreadTaskRunnerHandle::Get()));
  }

  bool WriteResponse(ServiceWorkerDiskCache* disk_cache,
                     const ResponseData& response) {
    scoped_ptr<ServiceWorkerResponseWriter> writer(
        new ServiceWorkerResponseWriter(response.resource_id, disk_cache));

    // Write the response info.
    scoped_ptr<net::HttpResponseInfo> info(new net::HttpResponseInfo);
    info->request_time = base::Time() + base::TimeDelta::FromSeconds(10);
    info->response_time = info->request_time + base::TimeDelta::FromSeconds(10);
    info->was_cached = false;
    info->headers = new net::HttpResponseHeaders(response.headers);
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
        new HttpResponseInfoIOBuffer(info.release());
    net::TestCompletionCallback cb1;
    writer->WriteInfo(info_buffer.get(), cb1.callback());
    int rv = cb1.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv < 0)
      return false;

    // Write the response metadata.
    scoped_ptr<ServiceWorkerResponseMetadataWriter> metadata_writer(
        new ServiceWorkerResponseMetadataWriter(response.resource_id,
                                                disk_cache));
    scoped_refptr<net::IOBuffer> metadata_buffer(
        new net::WrappedIOBuffer(response.metadata.data()));
    const int metadata_length = response.metadata.length();
    net::TestCompletionCallback cb2;
    metadata_writer->WriteMetadata(metadata_buffer.get(), metadata_length,
                                   cb2.callback());
    rv = cb2.WaitForResult();
    EXPECT_EQ(metadata_length, rv);
    if (metadata_length != rv)
      return false;

    // Write the response body.
    scoped_refptr<net::IOBuffer> body_buffer(
        new net::WrappedIOBuffer(response.body.data()));
    const int body_length = response.body.length();
    net::TestCompletionCallback cb3;
    writer->WriteData(body_buffer.get(), body_length, cb3.callback());
    rv = cb3.WaitForResult();
    EXPECT_EQ(body_length, rv);
    if (body_length != rv)
      return false;

    return true;
  }

  void VerifyResponse(ServiceWorkerDiskCache* disk_cache,
                      const ResponseData& expected) {
    scoped_ptr<ServiceWorkerResponseReader> reader(
        new ServiceWorkerResponseReader(expected.resource_id, disk_cache));

    // Verify the response info.
    scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
        new HttpResponseInfoIOBuffer;
    net::TestCompletionCallback cb1;
    reader->ReadInfo(info_buffer.get(), cb1.callback());
    int rv = cb1.WaitForResult();
    EXPECT_LT(0, rv);
    ASSERT_TRUE(info_buffer->http_info);
    ASSERT_TRUE(info_buffer->http_info->headers);
    EXPECT_EQ("OK", info_buffer->http_info->headers->GetStatusText());

    // Verify the response metadata.
    if (!expected.metadata.empty()) {
      ASSERT_TRUE(info_buffer->http_info->metadata);
      const int data_size = info_buffer->http_info->metadata->size();
      ASSERT_EQ(static_cast<int>(expected.metadata.length()), data_size);
      EXPECT_EQ(0, memcmp(expected.metadata.data(),
                          info_buffer->http_info->metadata->data(),
                          expected.metadata.length()));
    }

    // Verify the response body.
    const int kBigEnough = 512;
    scoped_refptr<net::IOBuffer> body_buffer = new net::IOBuffer(kBigEnough);
    net::TestCompletionCallback cb2;
    reader->ReadData(body_buffer.get(), kBigEnough, cb2.callback());
    rv = cb2.WaitForResult();
    ASSERT_EQ(static_cast<int>(expected.body.length()), rv);
    EXPECT_EQ(0, memcmp(expected.body.data(), body_buffer->data(), rv));
  }

  int32 GetEntryCount(ServiceWorkerDiskCache* disk_cache) {
    return disk_cache->disk_cache()->GetEntryCount();
  }

  ServiceWorkerStorage* storage() { return context_->storage(); }

 private:
  TestBrowserThreadBundle browser_thread_bundle_;
  base::ScopedTempDir user_data_directory_;

  scoped_ptr<ServiceWorkerContextCore> context_;
};

TEST_F(ServiceWorkerDiskCacheMigratorTest, MigrateDiskCache) {
  std::vector<ResponseData> responses;
  responses.push_back(ResponseData(1, "HTTP/1.1 200 OK\0\0", "Hello", ""));
  responses.push_back(ResponseData(2, "HTTP/1.1 200 OK\0\0", "Service", ""));
  responses.push_back(ResponseData(5, "HTTP/1.1 200 OK\0\0", "Worker", ""));
  responses.push_back(ResponseData(3, "HTTP/1.1 200 OK\0\0", "World", "meta"));
  responses.push_back(ResponseData(10, "HTTP/1.1 200 OK\0\0", "", "meta"));
  responses.push_back(ResponseData(11, "HTTP/1.1 200 OK\0\0", "body", ""));
  responses.push_back(ResponseData(12, "HTTP/1.1 200 OK\0\0", "", ""));
  responses.push_back(ResponseData(
      20, "HTTP/1.1 200 OK\0\0", std::string(256, 'a'), std::string(128, 'b')));

  // Populate initial data in the src diskcache.
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  for (const ResponseData& response : responses) {
    ASSERT_TRUE(WriteResponse(src.get(), response));
    VerifyResponse(src.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  src.reset();

  // Start the migrator.
  base::RunLoop run_loop;
  scoped_ptr<ServiceWorkerDiskCacheMigrator> migrator(CreateMigrator());
  migrator->Start(base::Bind(&OnDiskCacheMigrated, run_loop.QuitClosure()));
  run_loop.Run();

  // Verify the migrated contents in the dest diskcache.
  scoped_ptr<ServiceWorkerDiskCache> dest(CreateDestDiskCache());
  for (const ResponseData& response : responses)
    VerifyResponse(dest.get(), response);
  EXPECT_EQ(static_cast<int>(responses.size()), GetEntryCount(dest.get()));
}

TEST_F(ServiceWorkerDiskCacheMigratorTest, MigrateEmptyDiskCache) {
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  ASSERT_EQ(0, GetEntryCount(src.get()));
  src.reset();

  // Start the migrator.
  base::RunLoop run_loop;
  scoped_ptr<ServiceWorkerDiskCacheMigrator> migrator(CreateMigrator());
  migrator->Start(base::Bind(&OnDiskCacheMigrated, run_loop.QuitClosure()));
  run_loop.Run();

  scoped_ptr<ServiceWorkerDiskCache> dest(CreateDestDiskCache());
  ASSERT_EQ(0, GetEntryCount(dest.get()));
}

// Tests that the migrator properly removes existing resources in the dest
// diskcache before starting the migration.
TEST_F(ServiceWorkerDiskCacheMigratorTest, RemoveExistingResourcesFromDest) {
  std::vector<ResponseData> responses1;
  responses1.push_back(ResponseData(1, "HTTP/1.1 200 OK\0\0", "Hello", ""));
  responses1.push_back(ResponseData(3, "HTTP/1.1 200 OK\0\0", "World", ""));

  std::vector<ResponseData> responses2;
  responses2.push_back(ResponseData(10, "HTTP/1.1 200 OK\0\0", "Hello", ""));
  responses2.push_back(ResponseData(11, "HTTP/1.1 200 OK\0\0", "Service", ""));
  responses2.push_back(ResponseData(12, "HTTP/1.1 200 OK\0\0", "", "Worker"));

  // Populate initial resources in the src diskcache.
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  for (const ResponseData& response : responses1) {
    ASSERT_TRUE(WriteResponse(src.get(), response));
    VerifyResponse(src.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses1.size()), GetEntryCount(src.get()));
  src.reset();

  // Populate different resources in the dest diskcache in order to simulate
  // a previous partial migration.
  scoped_ptr<ServiceWorkerDiskCache> dest(CreateDestDiskCache());
  for (const ResponseData& response : responses2) {
    ASSERT_TRUE(WriteResponse(dest.get(), response));
    VerifyResponse(dest.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses2.size()), GetEntryCount(dest.get()));
  dest.reset();

  // Start the migrator.
  base::RunLoop run_loop;
  scoped_ptr<ServiceWorkerDiskCacheMigrator> migrator(CreateMigrator());
  migrator->Start(base::Bind(&OnDiskCacheMigrated, run_loop.QuitClosure()));
  run_loop.Run();

  // Verify that only newly migrated resources exist in the dest diskcache.
  dest = CreateDestDiskCache();
  for (const ResponseData& response : responses1)
    VerifyResponse(dest.get(), response);
  EXPECT_EQ(static_cast<int>(responses1.size()), GetEntryCount(dest.get()));
}

TEST_F(ServiceWorkerDiskCacheMigratorTest, ThrottleInflightTasks) {
  std::vector<ResponseData> responses;
  for (int i = 0; i < 10; ++i)
    responses.push_back(ResponseData(i, "HTTP/1.1 200 OK\0\0", "foo", "bar"));

  // Populate initial data in the src diskcache.
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  for (const ResponseData& response : responses) {
    ASSERT_TRUE(WriteResponse(src.get(), response));
    VerifyResponse(src.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  src.reset();

  scoped_ptr<ServiceWorkerDiskCacheMigrator> migrator(CreateMigrator());

  // Tighten the max number of inflight tasks.
  migrator->set_max_number_of_inflight_tasks(2);

  // Migration should hit the limit, but should successfully complete.
  base::RunLoop run_loop;
  migrator->Start(base::Bind(&OnDiskCacheMigrated, run_loop.QuitClosure()));
  run_loop.Run();

  // Verify the migrated contents in the dest diskcache.
  scoped_ptr<ServiceWorkerDiskCache> dest(CreateDestDiskCache());
  for (const ResponseData& response : responses)
    VerifyResponse(dest.get(), response);
  EXPECT_EQ(static_cast<int>(responses.size()), GetEntryCount(dest.get()));
}

TEST_F(ServiceWorkerDiskCacheMigratorTest, MigrateOnDiskCacheAccess) {
  std::vector<ResponseData> responses;
  responses.push_back(ResponseData(1, "HTTP/1.1 200 OK\0\0", "Hello", ""));
  responses.push_back(ResponseData(2, "HTTP/1.1 200 OK\0\0", "Service", ""));
  responses.push_back(ResponseData(5, "HTTP/1.1 200 OK\0\0", "Worker", ""));
  responses.push_back(ResponseData(3, "HTTP/1.1 200 OK\0\0", "World", "meta"));

  // Populate initial resources in the src diskcache.
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  for (const ResponseData& response : responses) {
    ASSERT_TRUE(WriteResponse(src.get(), response));
    VerifyResponse(src.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetOldDiskCachePath()));
  src.reset();

  scoped_ptr<ServiceWorkerDatabase> database(
      new ServiceWorkerDatabase(storage()->GetDatabasePath()));

  // This is necessary to make the storage schedule diskcache migration.
  database->set_skip_writing_diskcache_migration_state_on_init_for_testing();

  // Simulate an existing database.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(ServiceWorkerDatabase::ResourceRecord(
      1, GURL("https://example.com/foo"), 10));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64> newly_purgeable_resources;
  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = 100;
  data.scope = GURL("https://example.com/");
  data.script = GURL("https://example.com/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));
  database.reset();

  // LazyInitialize() reads initial data and should schedule to migrate.
  ASSERT_FALSE(storage()->disk_cache_migration_needed_);
  base::RunLoop run_loop;
  storage()->LazyInitialize(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(storage()->disk_cache_migration_needed_);

  // DiskCache access should start the migration.
  ServiceWorkerDiskCache* dest = storage()->disk_cache();

  // Verify the migrated contents in the dest diskcache.
  for (const ResponseData& response : responses)
    VerifyResponse(dest, response);
  EXPECT_EQ(static_cast<int>(responses.size()), GetEntryCount(dest));

  // After the migration, the src diskcache should be deleted.
  EXPECT_FALSE(base::DirectoryExists(storage()->GetOldDiskCachePath()));

  // After the migration, the migration state should be updated.
  bool migration_needed = false;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      storage()->database_->IsDiskCacheMigrationNeeded(&migration_needed));
  EXPECT_FALSE(migration_needed);
  bool deletion_needed = false;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      storage()->database_->IsOldDiskCacheDeletionNeeded(&deletion_needed));
  EXPECT_FALSE(deletion_needed);
}

TEST_F(ServiceWorkerDiskCacheMigratorTest, NotMigrateOnDatabaseAccess) {
  std::vector<ResponseData> responses;
  responses.push_back(ResponseData(1, "HTTP/1.1 200 OK\0\0", "Hello", ""));
  responses.push_back(ResponseData(2, "HTTP/1.1 200 OK\0\0", "Service", ""));
  responses.push_back(ResponseData(5, "HTTP/1.1 200 OK\0\0", "Worker", ""));
  responses.push_back(ResponseData(3, "HTTP/1.1 200 OK\0\0", "World", "meta"));

  // Populate initial resources in the src diskcache.
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  for (const ResponseData& response : responses) {
    ASSERT_TRUE(WriteResponse(src.get(), response));
    VerifyResponse(src.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetOldDiskCachePath()));

  scoped_ptr<ServiceWorkerDatabase> database(
      new ServiceWorkerDatabase(storage()->GetDatabasePath()));

  // This is necessary to make the storage schedule diskcache migration.
  database->set_skip_writing_diskcache_migration_state_on_init_for_testing();

  // Simulate an existing database.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(ServiceWorkerDatabase::ResourceRecord(
      1, GURL("https://example.com/foo"), 10));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64> newly_purgeable_resources;
  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = 100;
  data.scope = GURL("https://example.com/");
  data.script = GURL("https://example.com/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            database->WriteRegistration(data, resources, &deleted_version,
                                        &newly_purgeable_resources));
  database.reset();

  // LazyInitialize() reads initial data and should schedule to migrate.
  ASSERT_FALSE(storage()->disk_cache_migration_needed_);
  base::RunLoop run_loop1;
  storage()->LazyInitialize(run_loop1.QuitClosure());
  run_loop1.Run();
  EXPECT_TRUE(storage()->disk_cache_migration_needed_);

  // Database access should not start the migration.
  base::RunLoop run_loop2;
  storage()->FindRegistrationForDocument(
      GURL("http://example.com/"),
      base::Bind(&OnRegistrationFound, run_loop2.QuitClosure()));
  run_loop2.Run();

  // Verify that the migration didn't happen.
  scoped_ptr<ServiceWorkerDiskCache> dest(CreateDestDiskCache());
  EXPECT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  EXPECT_EQ(0, GetEntryCount(dest.get()));
  EXPECT_TRUE(base::DirectoryExists(storage()->GetOldDiskCachePath()));
}

TEST_F(ServiceWorkerDiskCacheMigratorTest, NotMigrateForEmptyDatabase) {
  std::vector<ResponseData> responses;
  responses.push_back(ResponseData(1, "HTTP/1.1 200 OK\0\0", "Hello", ""));
  responses.push_back(ResponseData(2, "HTTP/1.1 200 OK\0\0", "Service", ""));
  responses.push_back(ResponseData(5, "HTTP/1.1 200 OK\0\0", "Worker", ""));
  responses.push_back(ResponseData(3, "HTTP/1.1 200 OK\0\0", "World", "meta"));

  // Populate initial resources in the src diskcache.
  scoped_ptr<ServiceWorkerDiskCache> src(CreateSrcDiskCache());
  for (const ResponseData& response : responses) {
    ASSERT_TRUE(WriteResponse(src.get(), response));
    VerifyResponse(src.get(), response);
  }
  ASSERT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  ASSERT_TRUE(base::DirectoryExists(storage()->GetOldDiskCachePath()));
  src.reset();

  // LazyInitialize() reads initial data and should not schedule to migrate
  // because the database is empty.
  ASSERT_FALSE(storage()->disk_cache_migration_needed_);
  base::RunLoop run_loop;
  storage()->LazyInitialize(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(storage()->disk_cache_migration_needed_);

  // DiskCache access should not start the migration.
  ServiceWorkerDiskCache* dest = storage()->disk_cache();

  // Verify that the migration didn't happen.
  src = CreateSrcDiskCache();
  for (const ResponseData& response : responses)
    VerifyResponse(src.get(), response);
  EXPECT_EQ(static_cast<int>(responses.size()), GetEntryCount(src.get()));
  EXPECT_TRUE(base::DirectoryExists(storage()->GetOldDiskCachePath()));
  EXPECT_EQ(0, GetEntryCount(dest));

  // Write a registration into the database to start database initialization.
  std::vector<ServiceWorkerDatabase::ResourceRecord> resources;
  resources.push_back(ServiceWorkerDatabase::ResourceRecord(
      1, GURL("https://example.com/foo"), 10));
  ServiceWorkerDatabase::RegistrationData deleted_version;
  std::vector<int64> newly_purgeable_resources;
  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = 100;
  data.scope = GURL("https://example.com/");
  data.script = GURL("https://example.com/script.js");
  data.version_id = 200;
  data.resources_total_size_bytes = 10;
  ASSERT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->WriteRegistration(
                data, resources, &deleted_version, &newly_purgeable_resources));

  // After the database initialization, the migration state should be
  // initialized.
  bool migration_needed = false;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      storage()->database_->IsDiskCacheMigrationNeeded(&migration_needed));
  EXPECT_FALSE(migration_needed);
  bool deletion_needed = false;
  EXPECT_EQ(
      ServiceWorkerDatabase::STATUS_OK,
      storage()->database_->IsOldDiskCacheDeletionNeeded(&deletion_needed));

  // The deletion flag should be set for the case that the database is empty but
  // the old diskcache exists.
  EXPECT_TRUE(deletion_needed);
}

}  // namespace content
