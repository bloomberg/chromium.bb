// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kAppID[] = "app_id";

}  // namespace

class MockExtensionService : public TestExtensionService {
 public:
  MockExtensionService() {}
  virtual ~MockExtensionService() {}

  virtual const ExtensionSet* extensions() const OVERRIDE {
    return &extensions_;
  }

  virtual const ExtensionSet* disabled_extensions() const OVERRIDE {
    return &disabled_extensions_;
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
  ExtensionSet extensions_;
  ExtensionSet disabled_extensions_;

  DISALLOW_COPY_AND_ASSIGN(MockExtensionService);
};

class SyncEngineTest : public testing::Test {
 public:
  SyncEngineTest() {}
  virtual ~SyncEngineTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    extension_service_.reset(new MockExtensionService);
    scoped_ptr<drive::FakeDriveService> fake_drive_service(
        new drive::FakeDriveService);

    ASSERT_TRUE(fake_drive_service->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service->LoadResourceListForWapi(
        "gdata/empty_feed.json"));
    sync_engine_.reset(new drive_backend::SyncEngine(
        profile_dir_.path(),
        base::MessageLoopProxy::current(),
        fake_drive_service.PassAs<drive::DriveServiceInterface>(),
        NULL,
        extension_service_.get()));
    sync_engine_->Initialize();
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

 private:
  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir profile_dir_;

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

}  // namespace drive_backend
}  // namespace sync_file_system
