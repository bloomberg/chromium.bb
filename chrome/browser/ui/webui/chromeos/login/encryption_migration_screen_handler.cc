// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"

#include <string>
#include <utility>

#include "ash/system/devicetype_utils.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

constexpr char kJsScreenPath[] = "login.EncryptionMigrationScreen";

// Path to the mount point to check the available space.
constexpr char kCheckStoragePath[] = "/home";

// The minimum size of available space to start the migration.
constexpr int64_t kMinimumAvailableStorage = 10LL * 1024 * 1024;  // 10MB

// The minimum battery level to start the migration.
constexpr double kMinimumBatteryPercent = 30;

// JS API callbacks names.
constexpr char kJsApiStartMigration[] = "startMigration";
constexpr char kJsApiSkipMigration[] = "skipMigration";
constexpr char kJsApiRequestRestart[] = "requestRestart";

bool IsTestingUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kTestEncryptionMigrationUI);
}

}  // namespace

namespace chromeos {

EncryptionMigrationScreenHandler::EncryptionMigrationScreenHandler()
    : BaseScreenHandler(kScreenId), weak_ptr_factory_(this) {
  set_call_js_prefix(kJsScreenPath);
}

EncryptionMigrationScreenHandler::~EncryptionMigrationScreenHandler() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void EncryptionMigrationScreenHandler::Show() {
  if (!page_is_ready() || !delegate_) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void EncryptionMigrationScreenHandler::Hide() {
  show_on_init_ = false;
}

void EncryptionMigrationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void EncryptionMigrationScreenHandler::SetUserContext(
    const UserContext& user_context) {
  user_context_ = user_context;
}

void EncryptionMigrationScreenHandler::SetShouldResume(bool should_resume) {
  should_resume_ = should_resume;
  CallJS("setIsResuming", should_resume_);
}

void EncryptionMigrationScreenHandler::SetContinueLoginCallback(
    ContinueLoginCallback callback) {
  continue_login_callback_ = std::move(callback);
}

void EncryptionMigrationScreenHandler::SetupInitialView() {
  CheckAvailableStorage();
}

void EncryptionMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("migrationReadyTitle", IDS_ENCRYPTION_MIGRATION_READY_TITLE);
  builder->Add("migrationReadyDescription",
               ash::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_READY_DESCRIPTION));
  builder->Add("migrationMigratingTitle",
               IDS_ENCRYPTION_MIGRATION_MIGRATING_TITLE);
  builder->Add("migrationMigratingDescription",
               ash::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_MIGRATING_DESCRIPTION));
  builder->Add("migrationProgressLabel",
               IDS_ENCRYPTION_MIGRATION_PROGRESS_LABEL);
  builder->Add("migrationBatteryWarningLabel",
               IDS_ENCRYPTION_MIGRATION_BATTERY_WARNING_LABEL);
  builder->Add("migrationAskChargeMessage",
               ash::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_ASK_CHARGE_MESSAGE));
  builder->Add("migrationNecessaryBatteryLevelLabel",
               IDS_ENCRYPTION_MIGRATION_NECESSARY_BATTERY_LEVEL_MESSAGE);
  builder->Add("migrationChargingLabel",
               IDS_ENCRYPTION_MIGRATION_CHARGING_LABEL);
  builder->Add("migrationFailedTitle", IDS_ENCRYPTION_MIGRATION_FAILED_TITLE);
  builder->Add("migrationFailedSubtitle",
               IDS_ENCRYPTION_MIGRATION_FAILED_SUBTITLE);
  builder->Add("migrationFailedMessage",
               ash::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_FAILED_MESSAGE));
  builder->Add("migrationNospaceWarningLabel",
               IDS_ENCRYPTION_MIGRATION_NOSPACE_WARNING_LABEL);
  builder->Add("migrationAskFreeSpaceMessage",
               IDS_ENCRYPTION_MIGRATION_ASK_FREE_SPACE_MESSAGE);
  builder->Add("migrationAvailableSpaceLabel",
               IDS_ENCRYPTION_MIGRATION_AVAILABLE_SPACE_LABEL);
  builder->Add("migrationNecessarySpaceLabel",
               IDS_ENCRYPTION_MIGRATION_NECESSARY_SPACE_LABEL);
  builder->Add("migrationButtonUpdate", IDS_ENCRYPTION_MIGRATION_BUTTON_UPDATE);
  builder->Add("migrationButtonSkip", IDS_ENCRYPTION_MIGRATION_BUTTON_SKIP);
  builder->Add("migrationButtonRestart",
               IDS_ENCRYPTION_MIGRATION_BUTTON_RESTART);
  builder->Add("migrationButtonContinue",
               IDS_ENCRYPTION_MIGRATION_BUTTON_CONTINUE);
  builder->Add("migrationButtonSignIn", IDS_ENCRYPTION_MIGRATION_BUTTON_SIGNIN);
}

void EncryptionMigrationScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EncryptionMigrationScreenHandler::RegisterMessages() {
  AddCallback(kJsApiStartMigration,
              &EncryptionMigrationScreenHandler::HandleStartMigration);
  AddCallback(kJsApiSkipMigration,
              &EncryptionMigrationScreenHandler::HandleSkipMigration);
  AddCallback(kJsApiRequestRestart,
              &EncryptionMigrationScreenHandler::HandleRequestRestart);
}

void EncryptionMigrationScreenHandler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  current_battery_percent_ = proto.battery_percent();
  CallJS("setBatteryState", current_battery_percent_,
         current_battery_percent_ >= kMinimumBatteryPercent,
         proto.battery_state() ==
             power_manager::PowerSupplyProperties_BatteryState_CHARGING);

  // If the migration was already requested and the bettery level is enough now,
  // The migration should start immediately.
  if (current_battery_percent_ >= kMinimumBatteryPercent &&
      should_migrate_on_enough_battery_) {
    should_migrate_on_enough_battery_ = false;
    StartMigration();
  }
}

void EncryptionMigrationScreenHandler::HandleStartMigration() {
  WaitBatteryAndMigrate();
}

void EncryptionMigrationScreenHandler::HandleSkipMigration() {
  // If the user skips migration, we mount the cryptohome without performing the
  // migration by reusing UserContext and LoginPerformer which were used in the
  // previous attempt and dropping |is_forcing_dircrypto| flag in UserContext.
  // In this case, the user can not launch ARC apps in the session, and will be
  // asked to do the migration again in the next log-in attempt.
  if (!continue_login_callback_.is_null()) {
    user_context_.SetIsForcingDircrypto(false);
    std::move(continue_login_callback_).Run(user_context_);
  }
}

void EncryptionMigrationScreenHandler::HandleRequestRestart() {
  chrome::AttemptRestart();
}

void EncryptionMigrationScreenHandler::UpdateUIState(UIState state) {
  if (state == current_ui_state_)
    return;

  current_ui_state_ = state;
  CallJS("setUIState", static_cast<int>(state));

  // When this handler is about to show the READY screen, we should get the
  // latest battery status and show it on the screen.
  if (state == UIState::READY)
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();
}

void EncryptionMigrationScreenHandler::CheckAvailableStorage() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits().MayBlock().WithPriority(
          base::TaskPriority::USER_VISIBLE),
      base::Bind(&base::SysInfo::AmountOfFreeDiskSpace,
                 base::FilePath(kCheckStoragePath)),
      base::Bind(&EncryptionMigrationScreenHandler::OnGetAvailableStorage,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnGetAvailableStorage(int64_t size) {
  if (size >= kMinimumAvailableStorage || IsTestingUI()) {
    if (should_resume_) {
      WaitBatteryAndMigrate();
    } else {
      UpdateUIState(UIState::READY);
    }
  } else {
    CallJS("setAvailableSpaceInString", ui::FormatBytes(size));
    CallJS("setNecessarySpaceInString",
           ui::FormatBytes(kMinimumAvailableStorage));
    UpdateUIState(UIState::NOT_ENOUGH_STORAGE);
  }
}

void EncryptionMigrationScreenHandler::WaitBatteryAndMigrate() {
  if (current_battery_percent_ >= kMinimumBatteryPercent) {
    StartMigration();
    return;
  }
  UpdateUIState(UIState::READY);

  should_migrate_on_enough_battery_ = true;
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();
}

void EncryptionMigrationScreenHandler::StartMigration() {
  UpdateUIState(UIState::MIGRATING);

  // Mount the existing eCryptfs vault to a temporary location for migration.
  cryptohome::MountParameters mount(false);
  mount.to_migrate_from_ecryptfs = true;
  cryptohome::HomedirMethods::GetInstance()->MountEx(
      cryptohome::Identification(user_context_.GetAccountId()),
      cryptohome::Authorization(GetAuthKey()), mount,
      base::Bind(&EncryptionMigrationScreenHandler::OnMountExistingVault,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnMountExistingVault(
    bool success,
    cryptohome::MountError return_code,
    const std::string& mount_hash) {
  if (!success || return_code != cryptohome::MOUNT_ERROR_NONE) {
    UpdateUIState(UIState::MIGRATION_FAILED);
    return;
  }

  DBusThreadManager::Get()
      ->GetCryptohomeClient()
      ->SetDircryptoMigrationProgressHandler(
          base::Bind(&EncryptionMigrationScreenHandler::OnMigrationProgress,
                     weak_ptr_factory_.GetWeakPtr()));
  cryptohome::HomedirMethods::GetInstance()->MigrateToDircrypto(
      cryptohome::Identification(user_context_.GetAccountId()),
      base::Bind(&EncryptionMigrationScreenHandler::OnMigrationRequested,
                 weak_ptr_factory_.GetWeakPtr()));
}

cryptohome::KeyDefinition EncryptionMigrationScreenHandler::GetAuthKey() {
  // |auth_key| is created in the same manner as CryptohomeAuthenticator.
  const Key* key = user_context_.GetKey();
  // If the |key| is a plain text password, crash rather than attempting to
  // mount the cryptohome with a plain text password.
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  // Set the authentication's key label to an empty string, which is a wildcard
  // allowing any key to match. This is necessary because cryptohomes created by
  // Chrome OS M38 and older will have a legacy key with no label while those
  // created by Chrome OS M39 and newer will have a key with the label
  // kCryptohomeGAIAKeyLabel.
  return cryptohome::KeyDefinition(key->GetSecret(), std::string(),
                                   cryptohome::PRIV_DEFAULT);
}

void EncryptionMigrationScreenHandler::OnMigrationProgress(
    cryptohome::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  switch (status) {
    case cryptohome::DIRCRYPTO_MIGRATION_INITIALIZING:
      UpdateUIState(UIState::MIGRATING);
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_IN_PROGRESS:
      UpdateUIState(UIState::MIGRATING);
      CallJS("setMigrationProgress", static_cast<double>(current) / total);
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_SUCCESS:
      // Restart immediately after successful migration.
      chrome::AttemptRestart();
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_FAILED:
      UpdateUIState(UIState::MIGRATION_FAILED);
      // Stop listening to the progress updates.
      DBusThreadManager::Get()
          ->GetCryptohomeClient()
          ->SetDircryptoMigrationProgressHandler(
              CryptohomeClient::DircryptoMigrationProgessHandler());
      break;
    default:
      break;
  }
}

void EncryptionMigrationScreenHandler::OnMigrationRequested(bool success) {
  // This function is called when MigrateToDircrypto is correctly requested.
  // It does not mean that the migration is completed. We should know the
  // completion by DircryptoMigrationProgressHandler. success == false means a
  // failure in DBus communication.
  // TODO(fukino): Handle this case. Should we retry or restart?
  DCHECK(success);
}

}  // namespace chromeos
