// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kAppID[] = "app_id";

}  // namespace

class SyncEngineTest : public testing::Test {
 public:
  SyncEngineTest() {}
  virtual ~SyncEngineTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
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
        NULL, NULL));
    sync_engine_->Initialize();
    base::RunLoop().RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    sync_engine_.reset();
    base::RunLoop().RunUntilIdle();
  }

  SyncEngine* sync_engine() { return sync_engine_.get(); }

 private:
  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir profile_dir_;

  scoped_ptr<drive_backend::SyncEngine> sync_engine_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineTest);
};

TEST_F(SyncEngineTest, EnableOrigin) {
  base::RunLoop run_loop;
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

}  // namespace drive_backend
}  // namespace sync_file_system
