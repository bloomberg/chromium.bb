// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/auth/user_context.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace device {
class PowerSaveBlocker;
}  // namespace device

namespace chromeos {

class LoginFeedback;

// WebUI implementation of EncryptionMigrationScreenView
class EncryptionMigrationScreenHandler : public EncryptionMigrationScreenView,
                                         public BaseScreenHandler,
                                         public PowerManagerClient::Observer {
 public:
  EncryptionMigrationScreenHandler();
  ~EncryptionMigrationScreenHandler() override;

  // EncryptionMigrationScreenView implementation:
  void Show() override;
  void Hide() override;
  void SetDelegate(Delegate* delegate) override;
  void SetUserContext(const UserContext& user_context) override;
  void SetShouldResume(bool should_resume) override;
  void SetContinueLoginCallback(ContinueLoginCallback callback) override;
  void SetupInitialView() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  // Enumeration for the migration state. These values must be kept in sync with
  // EncryptionMigrationUIState in JS code.
  enum UIState {
    INITIAL = 0,
    READY = 1,
    MIGRATING = 2,
    MIGRATION_FAILED = 3,
    NOT_ENOUGH_STORAGE = 4,
  };

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // PowerManagerClient::Observer implementation:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // Handlers for JS API callbacks.
  void HandleStartMigration();
  void HandleSkipMigration();
  void HandleRequestRestart();
  void HandleOpenFeedbackDialog();

  // Updates UI state.
  void UpdateUIState(UIState state);

  void CheckAvailableStorage();
  void OnGetAvailableStorage(int64_t size);
  void WaitBatteryAndMigrate();
  void StartMigration();
  void OnMountExistingVault(bool success,
                            cryptohome::MountError return_code,
                            const std::string& mount_hash);
  void StartBlockingPowerSave();
  void StopBlockingPowerSave();
  // Removes cryptohome and shows the error screen after the removal finishes.
  void RemoveCryptohome();
  void OnRemoveCryptohome(bool success, cryptohome::MountError return_code);

  // Creates authorization key for MountEx method using |user_context_|.
  cryptohome::KeyDefinition GetAuthKey();

  // Handlers for cryptohome API callbacks.
  void OnMigrationProgress(cryptohome::DircryptoMigrationStatus status,
                           uint64_t current,
                           uint64_t total);
  void OnMigrationRequested(bool success);

  Delegate* delegate_ = nullptr;
  bool show_on_init_ = false;

  // The current UI state which should be refrected in the web UI.
  UIState current_ui_state_ = INITIAL;

  // The current user's UserContext, which is used to request the migration to
  // cryptohome.
  UserContext user_context_;

  // The callback which is used to log in to the session from the migration UI.
  ContinueLoginCallback continue_login_callback_;

  // True if the system should resume the previous incomplete migration.
  bool should_resume_ = false;

  // The current battery level.
  double current_battery_percent_ = 0.0;

  // True if the migration should start immediately once the battery level gets
  // sufficient.
  bool should_migrate_on_enough_battery_ = false;

  // The battery level at the timing that the migration starts.
  double initial_battery_percent_ = 0.0;

  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;

  std::unique_ptr<LoginFeedback> login_feedback_;

  base::WeakPtrFactory<EncryptionMigrationScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EncryptionMigrationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
