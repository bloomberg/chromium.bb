// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kAppID[] = "app_id";

void EmptyTask(SyncStatusCode status, const SyncStatusCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, status));
}

}  // namespace

class MockSyncTask : public ExclusiveTask {
 public:
  explicit MockSyncTask(bool used_network) {
    set_used_network(used_network);
  }
  virtual ~MockSyncTask() {}

  virtual void RunExclusive(const SyncStatusCallback& callback) OVERRIDE {
    callback.Run(SYNC_STATUS_OK);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncTask);
};

class MockExtensionService : public TestExtensionService {
 public:
  MockExtensionService() {}
  virtual ~MockExtensionService() {}

  virtual const extensions::ExtensionSet* extensions() const OVERRIDE {
    return &extensions_;
  }

  virtual void AddExtension(const extensions::Extension* extension) OVERRIDE {
    extensions_.Insert(make_scoped_refptr(extension));
  }

  virtual const extensions::Extension* GetInstalledExtension(
      const std::string& extension_id) const OVERRIDE {
    return extensions_.GetByID(extension_id);
  }

  virtual bool IsExtensionEnabled(
      const std::string& extension_id) const OVERRIDE {
    return extensions_.Contains(extension_id) &&
        !disabled_extensions_.Contains(extension_id);
  }

  void UninstallExtension(const std::string& extension_id) {
    extensions_.Remove(extension_id);
    disabled_extensions_.Remove(extension_id);
  }

  void DisableExtension(const std::string& extension_id) {
    if (!IsExtensionEnabled(extension_id))
      return;
    const extensions::Extension* extension = extensions_.GetByID(extension_id);
    disabled_extensions_.Insert(make_scoped_refptr(extension));
  }

 private:
  extensions::ExtensionSet extensions_;
  extensions::ExtensionSet disabled_extensions_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionService);
};

class SyncEngineTest
    : public testing::Test,
      public base::SupportsWeakPtr<SyncEngineTest> {
 public:
  SyncEngineTest() {}
  virtual ~SyncEngineTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    extension_service_.reset(new MockExtensionService);
    scoped_ptr<drive::FakeDriveService> fake_drive_service(
        new drive::FakeDriveService);

    sync_engine_.reset(new drive_backend::SyncEngine(
        profile_dir_.path(),
        base::MessageLoopProxy::current(),
        fake_drive_service.PassAs<drive::DriveServiceInterface>(),
        scoped_ptr<drive::DriveUploaderInterface>(),
        NULL /* notification_manager */,
        extension_service_.get(),
        NULL /* signin_manager */,
        in_memory_env_.get()));
    sync_engine_->Initialize();
    sync_engine_->SetSyncEnabled(true);
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    sync_engine_.reset();
    extension_service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  MockExtensionService* extension_service() { return extension_service_.get(); }
  SyncEngine* sync_engine() { return sync_engine_.get(); }

  void UpdateRegisteredApps() {
    sync_engine_->UpdateRegisteredApps();
  }

  SyncTaskManager* GetSyncEngineTaskManager() {
    return sync_engine_->task_manager_.get();
  }

  void CheckServiceState(SyncStatusCode expected_sync_status,
                         RemoteServiceState expected_service_status,
                         SyncStatusCode sync_status) {
    EXPECT_EQ(expected_sync_status, sync_status);
    EXPECT_EQ(expected_service_status, sync_engine_->GetCurrentState());
  }

 private:
  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir profile_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;

  scoped_ptr<MockExtensionService> extension_service_;
  scoped_ptr<drive_backend::SyncEngine> sync_engine_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineTest);
};

TEST_F(SyncEngineTest, EnableOrigin) {
  FileTracker tracker;
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
  MetadataDatabase* metadata_database = sync_engine()->GetMetadataDatabase();
  GURL origin = extensions::Extension::GetBaseURLFromExtensionId(kAppID);

  sync_engine()->RegisterOrigin(origin, CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(metadata_database->FindAppRootTracker(kAppID, &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  sync_engine()->DisableOrigin(origin, CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(metadata_database->FindAppRootTracker(kAppID, &tracker));
  EXPECT_EQ(TRACKER_KIND_DISABLED_APP_ROOT, tracker.tracker_kind());

  sync_engine()->EnableOrigin(origin, CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_TRUE(metadata_database->FindAppRootTracker(kAppID, &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  sync_engine()->UninstallOrigin(
      origin,
      RemoteFileSyncService::UNINSTALL_AND_KEEP_REMOTE,
      CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  ASSERT_FALSE(metadata_database->FindAppRootTracker(kAppID, &tracker));
}

TEST_F(SyncEngineTest, UpdateRegisteredApps) {
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
  for (int i = 0; i < 3; i++) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
        .SetManifest(extensions::DictionaryBuilder()
                     .Set("name", "foo")
                     .Set("version", "1.0")
                     .Set("manifest_version", 2))
        .SetID(base::StringPrintf("app_%d", i))
        .Build();
    extension_service()->AddExtension(extension.get());
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(
        extension->id());
    sync_status = SYNC_STATUS_UNKNOWN;
    sync_engine()->RegisterOrigin(origin, CreateResultReceiver(&sync_status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, sync_status);
  }

  MetadataDatabase* metadata_database = sync_engine()->GetMetadataDatabase();
  FileTracker tracker;

  ASSERT_TRUE(metadata_database->FindAppRootTracker("app_0", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  ASSERT_TRUE(metadata_database->FindAppRootTracker("app_1", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  ASSERT_TRUE(metadata_database->FindAppRootTracker("app_2", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  extension_service()->DisableExtension("app_1");
  extension_service()->UninstallExtension("app_2");
  ASSERT_FALSE(extension_service()->GetInstalledExtension("app_2"));
  UpdateRegisteredApps();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_database->FindAppRootTracker("app_0", &tracker));
  EXPECT_EQ(TRACKER_KIND_APP_ROOT, tracker.tracker_kind());

  ASSERT_TRUE(metadata_database->FindAppRootTracker("app_1", &tracker));
  EXPECT_EQ(TRACKER_KIND_DISABLED_APP_ROOT, tracker.tracker_kind());

  ASSERT_FALSE(metadata_database->FindAppRootTracker("app_2", &tracker));
}

TEST_F(SyncEngineTest, GetOriginStatusMap) {
  FileTracker tracker;
  SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
  GURL origin = extensions::Extension::GetBaseURLFromExtensionId(kAppID);

  sync_engine()->RegisterOrigin(GURL("chrome-extension://app_0"),
                                     CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  sync_engine()->RegisterOrigin(GURL("chrome-extension://app_1"),
                                     CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  RemoteFileSyncService::OriginStatusMap status_map;
  sync_engine()->GetOriginStatusMap(&status_map);
  ASSERT_EQ(2u, status_map.size());
  EXPECT_EQ("Enabled", status_map[GURL("chrome-extension://app_0")]);
  EXPECT_EQ("Enabled", status_map[GURL("chrome-extension://app_1")]);

  sync_engine()->DisableOrigin(GURL("chrome-extension://app_1"),
                               CreateResultReceiver(&sync_status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SYNC_STATUS_OK, sync_status);

  sync_engine()->GetOriginStatusMap(&status_map);
  ASSERT_EQ(2u, status_map.size());
  EXPECT_EQ("Enabled", status_map[GURL("chrome-extension://app_0")]);
  EXPECT_EQ("Disabled", status_map[GURL("chrome-extension://app_1")]);
}

TEST_F(SyncEngineTest, UpdateServiceState) {
  EXPECT_EQ(REMOTE_SERVICE_OK, sync_engine()->GetCurrentState());

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_STATUS_AUTHENTICATION_FAILED),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_AUTHENTICATION_FAILED,
                 REMOTE_SERVICE_AUTHENTICATION_REQUIRED));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_STATUS_ACCESS_FORBIDDEN),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_ACCESS_FORBIDDEN,
                 REMOTE_SERVICE_AUTHENTICATION_REQUIRED));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE,
                 REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_STATUS_NETWORK_ERROR),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_NETWORK_ERROR,
                 REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_STATUS_ABORT),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_ABORT,
                 REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_STATUS_FAILED),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_FAILED,
                 REMOTE_SERVICE_TEMPORARY_UNAVAILABLE));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_DATABASE_ERROR_CORRUPTION),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_DATABASE_ERROR_CORRUPTION,
                 REMOTE_SERVICE_DISABLED));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_DATABASE_ERROR_IO_ERROR),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_DATABASE_ERROR_IO_ERROR,
                 REMOTE_SERVICE_DISABLED));

  GetSyncEngineTaskManager()->ScheduleTask(
      FROM_HERE,
      base::Bind(&EmptyTask, SYNC_DATABASE_ERROR_FAILED),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_DATABASE_ERROR_FAILED,
                 REMOTE_SERVICE_DISABLED));

  GetSyncEngineTaskManager()->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(new MockSyncTask(false)),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_OK,
                 REMOTE_SERVICE_DISABLED));

  GetSyncEngineTaskManager()->ScheduleSyncTask(
      FROM_HERE,
      scoped_ptr<SyncTask>(new MockSyncTask(true)),
      SyncTaskManager::PRIORITY_MED,
      base::Bind(&SyncEngineTest::CheckServiceState,
                 AsWeakPtr(),
                 SYNC_STATUS_OK,
                 REMOTE_SERVICE_OK));

  base::RunLoop().RunUntilIdle();
}

}  // namespace drive_backend
}  // namespace sync_file_system
