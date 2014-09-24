// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {

namespace {

class FakeDriveServiceFactory
    : public drive_backend::SyncEngine::DriveServiceFactory {
 public:
  explicit FakeDriveServiceFactory(
      drive::FakeDriveService::ChangeObserver* change_observer)
      : change_observer_(change_observer) {}
  virtual ~FakeDriveServiceFactory() {}

  virtual scoped_ptr<drive::DriveServiceInterface> CreateDriveService(
      OAuth2TokenService* oauth2_token_service,
      net::URLRequestContextGetter* url_request_context_getter,
      base::SequencedTaskRunner* blocking_task_runner) OVERRIDE {
    scoped_ptr<drive::FakeDriveService> drive_service(
        new drive::FakeDriveService);
    drive_service->AddChangeObserver(change_observer_);
    return drive_service.PassAs<drive::DriveServiceInterface>();
  }

 private:
  drive::FakeDriveService::ChangeObserver* change_observer_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveServiceFactory);
};

}  // namespace

class SyncFileSystemTest : public extensions::PlatformAppBrowserTest,
                           public drive::FakeDriveService::ChangeObserver {
 public:
  SyncFileSystemTest()
      : fake_drive_service_(NULL),
        local_service_(NULL),
        remote_service_(NULL) {
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    real_minimum_preserved_space_ =
        storage::QuotaManager::kMinimumPreserveForSystem;
    storage::QuotaManager::kMinimumPreserveForSystem = 0;
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    storage::QuotaManager::kMinimumPreserveForSystem =
        real_minimum_preserved_space_;
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  scoped_refptr<base::SequencedTaskRunner> MakeSequencedTaskRunner() {
    scoped_refptr<base::SequencedWorkerPool> worker_pool =
        content::BrowserThread::GetBlockingPool();

    return worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
        worker_pool->GetSequenceToken(),
        base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());

    SyncFileSystemServiceFactory* factory =
        SyncFileSystemServiceFactory::GetInstance();

    content::BrowserContext* context = browser()->profile();
    ExtensionServiceInterface* extension_service =
        extensions::ExtensionSystem::Get(context)->extension_service();

    scoped_ptr<drive_backend::SyncEngine::DriveServiceFactory>
        drive_service_factory(new FakeDriveServiceFactory(this));

    fake_signin_manager_.reset(new FakeSigninManagerForTesting(
        browser()->profile()));

    remote_service_ = new drive_backend::SyncEngine(
        base::ThreadTaskRunnerHandle::Get(),  // ui_task_runner
        MakeSequencedTaskRunner(),
        MakeSequencedTaskRunner(),
        base_dir_.path(),
        NULL,  // task_logger
        NULL,  // notification_manager
        extension_service,
        fake_signin_manager_.get(),  // signin_manager
        NULL,  // token_service
        NULL,  // request_context
        drive_service_factory.Pass(),
        in_memory_env_.get());
    remote_service_->SetSyncEnabled(true);
    factory->set_mock_remote_file_service(
        scoped_ptr<RemoteFileSyncService>(remote_service_));
  }

  // drive::FakeDriveService::ChangeObserver override.
  virtual void OnNewChangeAvailable() OVERRIDE {
    sync_engine()->OnNotificationReceived();
  }

  SyncFileSystemService* sync_file_system_service() {
    return SyncFileSystemServiceFactory::GetForProfile(browser()->profile());
  }

  drive_backend::SyncEngine* sync_engine() {
    return static_cast<drive_backend::SyncEngine*>(
        sync_file_system_service()->remote_service_.get());
  }

  LocalFileSyncService* local_file_sync_service() {
    return sync_file_system_service()->local_service_.get();
  }

  void SignIn() {
    fake_signin_manager_->SetAuthenticatedUsername("tester");
    sync_engine()->GoogleSigninSucceeded("test_account", "tester", "testing");
  }

  void SetSyncEnabled(bool enabled) {
    sync_file_system_service()->SetSyncEnabledForTesting(enabled);
  }

  void WaitUntilIdle() {
    base::RunLoop run_loop;
    sync_file_system_service()->CallOnIdleForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::ScopedTempDir base_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;

  scoped_ptr<FakeSigninManagerForTesting> fake_signin_manager_;

  drive::FakeDriveService* fake_drive_service_;
  LocalFileSyncService* local_service_;
  drive_backend::SyncEngine* remote_service_;

  int64 real_minimum_preserved_space_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemTest);
};

IN_PROC_BROWSER_TEST_F(SyncFileSystemTest, AuthorizationTest) {
  ExtensionTestMessageListener open_failure(
      "checkpoint: Failed to get syncfs", true);
  ExtensionTestMessageListener bar_created(
      "checkpoint: \"/bar\" created", true);
  ExtensionTestMessageListener foo_created(
      "checkpoint: \"/foo\" created", true);
  extensions::ResultCatcher catcher;

  LoadAndLaunchPlatformApp("sync_file_system/authorization_test", "Launched");

  // Application sync is disabled at the initial state.  Thus first
  // syncFilesystem.requestFileSystem call should fail.
  ASSERT_TRUE(open_failure.WaitUntilSatisfied());

  // Enable Application sync and let the app retry.
  SignIn();
  SetSyncEnabled(true);

  open_failure.Reply("resume");

  ASSERT_TRUE(foo_created.WaitUntilSatisfied());

  // The app creates a file "/foo", that should successfully sync to the remote
  // service.  Wait for the completion and resume the app.
  WaitUntilIdle();

  sync_engine()->GoogleSignedOut("test_account", std::string());
  foo_created.Reply("resume");

  ASSERT_TRUE(bar_created.WaitUntilSatisfied());

  // The app creates anohter file "/bar".  Since the user signed out from chrome
  // The synchronization should fail and the service state should be
  // AUTHENTICATION_REQUIRED.

  WaitUntilIdle();
  EXPECT_EQ(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
            sync_engine()->GetCurrentState());

  sync_engine()->GoogleSigninSucceeded("test_account", "tester", "testing");
  WaitUntilIdle();

  bar_created.Reply("resume");

  EXPECT_TRUE(catcher.GetNextResult());
}

}  // namespace sync_file_system
