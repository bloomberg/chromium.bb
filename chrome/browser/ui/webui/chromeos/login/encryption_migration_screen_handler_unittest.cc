// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/chromeos/arc/arc_migration_constants.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_mode.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/mock_homedir_methods.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::_;

namespace chromeos {
namespace {

// Fake WakeLock implementation, required by EncryptionMigrationScreenHandler.
class FakeWakeLock : public device::mojom::WakeLock {
 public:
  FakeWakeLock() {}
  ~FakeWakeLock() override {}

  // Implement device::mojom::WakeLock:
  void RequestWakeLock() override { has_wakelock_ = true; }
  void CancelWakeLock() override { has_wakelock_ = false; }
  void AddClient(device::mojom::WakeLockRequest request) override {}
  void ChangeType(device::mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {
    NOTIMPLEMENTED();
  }
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {
    std::move(callback).Run(has_wakelock_);
  }

  bool HasWakeLock() { return has_wakelock_; }

 private:
  bool has_wakelock_ = false;
};

// Allows access to testing-only methods of EncryptionMigrationScreenHandler.
class TestEncryptionMigrationScreenHandler
    : public EncryptionMigrationScreenHandler {
 public:
  TestEncryptionMigrationScreenHandler() {
    SetFreeDiskSpaceFetcherForTesting(base::BindRepeating(
        &TestEncryptionMigrationScreenHandler::FreeDiskSpaceFetcher,
        base::Unretained(this)));
    auto tick_clock = base::MakeUnique<base::SimpleTestTickClock>();
    testing_tick_clock_ = tick_clock.get();
    SetTickClockForTesting(std::move(tick_clock));
  }

  // Sets the testing WebUI.
  void set_test_web_ui(content::TestWebUI* test_web_ui) {
    set_web_ui(test_web_ui);
  }

  // Sets the free disk space seen by EncryptionMigrationScreenHandler.
  void set_free_disk_space(int64_t free_disk_space) {
    free_disk_space_ = free_disk_space;
  }

  // Returns the SimpleTestTickClock used to simulate time elapsed during
  // migration.
  base::SimpleTestTickClock* testing_tick_clock() {
    return testing_tick_clock_;
  }

  FakeWakeLock* fake_wake_lock() { return &fake_wake_lock_; }

 protected:
  // Override GetWakeLock -- returns our own FakeWakeLock.
  device::mojom::WakeLock* GetWakeLock() override { return &fake_wake_lock_; }

 private:
  // Used as free disk space fetcher callback.
  int64_t FreeDiskSpaceFetcher() { return free_disk_space_; }

  FakeWakeLock fake_wake_lock_;

  // Non-owned pointer. Tick clock used to simulate time elapsed during
  // migration. This is actually owned by the base class.
  base::SimpleTestTickClock* testing_tick_clock_;

  int64_t free_disk_space_;
};

class EncryptionMigrationScreenHandlerTest : public testing::Test {
 public:
  EncryptionMigrationScreenHandlerTest() = default;
  ~EncryptionMigrationScreenHandlerTest() override = default;

  void SetUp() override {
    // Set up a MockUserManager.
    MockUserManager* mock_user_manager = new NiceMock<MockUserManager>();
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(mock_user_manager));

    // This is used by EncryptionMigrationScreenHandler to mount the existing
    // cryptohome. Ownership of mock_homedir_methods_ is transferred to
    // HomedirMethods::InitializeForTesting.
    mock_homedir_methods_ = new cryptohome::MockHomedirMethods;
    cryptohome::HomedirMethods::InitializeForTesting(mock_homedir_methods_);

    // This is used by EncryptionMigrationScreenHandler to remove the existing
    // cryptohome. Ownership of mock_async_method_caller_ is transferred to
    // AsyncMethodCaller::InitializeForTesting.
    mock_async_method_caller_ = new cryptohome::MockAsyncMethodCaller;
    cryptohome::AsyncMethodCaller::InitializeForTesting(
        mock_async_method_caller_);

    // Set up fake DBusThreadManager parts.
    auto fake_cryptohome_client = base::MakeUnique<FakeCryptohomeClient>();
    fake_cryptohome_client_ = fake_cryptohome_client.get();
    DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::move(fake_cryptohome_client));

    DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        base::MakeUnique<FakePowerManagerClient>());

    DBusThreadManager::Initialize();

    PowerPolicyController::Initialize(
        DBusThreadManager::Get()->GetPowerManagerClient());

    // Build dummy user context.
    user_context_.SetAccountId(account_id_);
    user_context_.SetKey(
        Key(Key::KeyType::KEY_TYPE_SALTED_SHA256, "salt", "secret"));

    encryption_migration_screen_handler_ =
        base::MakeUnique<TestEncryptionMigrationScreenHandler>();
    encryption_migration_screen_handler_->set_test_web_ui(&test_web_ui_);
    encryption_migration_screen_handler_->SetContinueLoginCallback(
        base::BindOnce(&EncryptionMigrationScreenHandlerTest::OnContinueLogin,
                       base::Unretained(this)));
    encryption_migration_screen_handler_->SetRestartLoginCallback(
        base::BindOnce(&EncryptionMigrationScreenHandlerTest::OnRestartLogin,
                       base::Unretained(this)));
    encryption_migration_screen_handler_->set_free_disk_space(
        arc::kMigrationMinimumAvailableStorage);
    encryption_migration_screen_handler_->SetUserContext(user_context_);
  }

  void TearDown() override {
    encryption_migration_screen_handler_.reset();

    PowerPolicyController::Shutdown();
    DBusThreadManager::Shutdown();
    cryptohome::AsyncMethodCaller::Shutdown();
    cryptohome::HomedirMethods::Shutdown();
  }

  // Sets up expectation that the existing user home will be mounted for
  // migration using |mock_homedir_methods_|.
  void ExpectMountExistingVault(cryptohome::MountError mount_error) {
    EXPECT_CALL(
        *mock_homedir_methods_,
        MountEx(cryptohome::Identification(
                    user_context_.GetAccountId()) /* 0: id */,
                _ /* 1: auth */, _ /* 2: request */, _ /* 3: callback */))
        .WillOnce(WithArgs<2, 3>(Invoke(
            [mount_error](const cryptohome::MountRequest& mount_request,
                          cryptohome::HomedirMethods::MountCallback callback) {
              // Expect that the migration flag is set.
              EXPECT_TRUE(mount_request.to_migrate_from_ecryptfs());
              callback.Run(true /* success */, mount_error,
                           std::string() /* mount_hash */);
            })));
  }

  // Sets up expectation that migration will be started on
  // |mock_homedir_methods_|. If |expect_minimal_migration| is true, verifies
  // that minimal migration has been requested.
  void ExpectStartMigration(bool expect_minimal_migration) {
    EXPECT_CALL(
        *mock_homedir_methods_,
        MigrateToDircrypto(cryptohome::Identification(
                               user_context_.GetAccountId()) /* 0: id */,
                           _ /* 1: minimal_migration*/, _ /* 2: callback */))
        .WillOnce(WithArgs<1, 2>(
            Invoke([expect_minimal_migration](
                       bool minimal_migration,
                       const cryptohome::HomedirMethods::DBusResultCallback&
                           callback) {
              EXPECT_EQ(expect_minimal_migration, minimal_migration);
              // Call the callback immediately - actual result is sent later
              // using DircryptoMigrationProgressHandler.
              callback.Run(true /* success */);
            })));
  }

 protected:
  // Must be the first member.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
  cryptohome::MockHomedirMethods* mock_homedir_methods_ = nullptr;
  FakeCryptohomeClient* fake_cryptohome_client_ = nullptr;
  cryptohome::MockAsyncMethodCaller* mock_async_method_caller_ = nullptr;
  std::unique_ptr<TestEncryptionMigrationScreenHandler>
      encryption_migration_screen_handler_;
  content::TestWebUI test_web_ui_;

  // Will be set to true in ContinueLogin.
  bool continue_login_callback_called_ = false;
  // Will be set to true in RestartLogin.
  bool restart_login_callback_called_ = false;

  const AccountId account_id_ =
      AccountId::FromUserEmail(user_manager::kStubUserEmail);
  UserContext user_context_;

 private:
  // This will be called by EncryptionMigrationScreenHandler upon finished
  // minimal migration when sign-in should continue.
  void OnContinueLogin(const UserContext& user_context) {
    EXPECT_FALSE(continue_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";
    EXPECT_FALSE(restart_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";

    continue_login_callback_called_ = true;
  }

  // This will be called by EncryptionMigrationScreenHandler upon finished
  // minimal migration when the user should re-enter their password.
  void OnRestartLogin(const UserContext& user_context) {
    EXPECT_FALSE(continue_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";
    EXPECT_FALSE(restart_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";

    restart_login_callback_called_ = true;
  }
};

}  // namespace

// Tests handling of a minimal migration run that finishes immediately.
TEST_F(EncryptionMigrationScreenHandlerTest, MinimalMigration) {
  ExpectMountExistingVault(cryptohome::MountError::MOUNT_ERROR_NONE);
  ExpectStartMigration(true /* minimal_migration */);
  encryption_migration_screen_handler_->SetMode(
      EncryptionMigrationMode::START_MINIMAL_MIGRATION);
  encryption_migration_screen_handler_->SetupInitialView();

  scoped_task_environment_.RunUntilIdle();

  Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  EXPECT_TRUE(
      encryption_migration_screen_handler_->fake_wake_lock()->HasWakeLock());
  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
      0 /* current */, 0 /* total */);

  EXPECT_TRUE(continue_login_callback_called_);
  EXPECT_FALSE(
      encryption_migration_screen_handler_->fake_wake_lock()->HasWakeLock());
}

// Tests handling of a resumed minimal migration run. This should behave the
// same way that a freshly started minimal migration does (only UMA stats are
// different, but we don't test that at the moment).
TEST_F(EncryptionMigrationScreenHandlerTest, ResumeMinimalMigration) {
  ExpectMountExistingVault(cryptohome::MountError::MOUNT_ERROR_NONE);
  ExpectStartMigration(true /* minimal_migration */);
  encryption_migration_screen_handler_->SetMode(
      EncryptionMigrationMode::RESUME_MINIMAL_MIGRATION);
  encryption_migration_screen_handler_->SetupInitialView();

  scoped_task_environment_.RunUntilIdle();

  Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
      0 /* current */, 0 /* total */);

  EXPECT_TRUE(continue_login_callback_called_);
}

// Tests handling of a minimal migration run that takes a long time to finish.
// We expect that EncryptionMigrationScreenHandler will require the user to
// re-enter their password.
TEST_F(EncryptionMigrationScreenHandlerTest, MinimalMigrationSlow) {
  ExpectMountExistingVault(cryptohome::MountError::MOUNT_ERROR_NONE);
  ExpectStartMigration(true /* minimal_migration */);
  encryption_migration_screen_handler_->SetMode(
      EncryptionMigrationMode::START_MINIMAL_MIGRATION);
  encryption_migration_screen_handler_->SetupInitialView();

  scoped_task_environment_.RunUntilIdle();

  Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  encryption_migration_screen_handler_->testing_tick_clock()->Advance(
      base::TimeDelta::FromMinutes(1));
  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
      0 /* current */, 0 /* total */);

  EXPECT_TRUE(restart_login_callback_called_);
}

// Tests handling of a minimal migration run that fails.
TEST_F(EncryptionMigrationScreenHandlerTest, MinimalMigrationFails) {
  ExpectMountExistingVault(cryptohome::MountError::MOUNT_ERROR_NONE);
  ExpectStartMigration(true /* minimal_migration */);
  encryption_migration_screen_handler_->SetMode(
      EncryptionMigrationMode::START_MINIMAL_MIGRATION);
  encryption_migration_screen_handler_->SetupInitialView();

  scoped_task_environment_.RunUntilIdle();

  Mock::VerifyAndClearExpectations(mock_homedir_methods_);

  EXPECT_CALL(
      *mock_async_method_caller_,
      AsyncRemove(cryptohome::Identification(user_context_.GetAccountId()),
                  _ /* callback */));
  encryption_migration_screen_handler_->testing_tick_clock()->Advance(
      base::TimeDelta::FromMinutes(1));
  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_FAILED,
      0 /* current */, 0 /* total */);

  Mock::VerifyAndClearExpectations(mock_async_method_caller_);
}

}  // namespace chromeos
