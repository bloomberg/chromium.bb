// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"

#include <cmath>
#include <string>
#include <utility>

#include "ash/system/devicetype_utils.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_migration_constants.h"
#include "chrome/browser/chromeos/login/ui/login_feedback.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "device/power_save_blocker/power_save_blocker.h"
#include "ui/base/text/bytes_formatting.h"

namespace {

constexpr char kJsScreenPath[] = "login.EncryptionMigrationScreen";

// Path to the mount point to check the available space.
constexpr char kCheckStoragePath[] = "/home";

// JS API callbacks names.
constexpr char kJsApiStartMigration[] = "startMigration";
constexpr char kJsApiSkipMigration[] = "skipMigration";
constexpr char kJsApiRequestRestartOnLowStorage[] =
    "requestRestartOnLowStorage";
constexpr char kJsApiRequestRestartOnFailure[] = "requestRestartOnFailure";
constexpr char kJsApiOpenFeedbackDialog[] = "openFeedbackDialog";

// UMA names.
constexpr char kUmaNameFirstScreen[] = "Cryptohome.MigrationUI.FirstScreen";
constexpr char kUmaNameUserChoice[] = "Cryptohome.MigrationUI.UserChoice";
constexpr char kUmaNameMigrationResult[] =
    "Cryptohome.MigrationUI.MigrationResult";
constexpr char kUmaNameRemoveCryptohomeResult[] =
    "Cryptohome.MigrationUI.RemoveCryptohomeResult";
constexpr char kUmaNameConsumedBatteryPercent[] =
    "Cryptohome.MigrationUI.ConsumedBatteryPercent";
constexpr char kUmaNameVisibleScreen[] = "Cryptohome.MigrationUI.VisibleScreen";

// This enum must match the numbering for MigrationUIFirstScreen in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before FIRST_SCREEN_COUNT.
enum class FirstScreen {
  FIRST_SCREEN_READY = 0,
  FIRST_SCREEN_RESUME = 1,
  FIRST_SCREEN_LOW_STORAGE = 2,
  FIRST_SCREEN_COUNT
};

// This enum must match the numbering for MigrationUIUserChoice in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before USER_CHOICE_COUNT.
enum class UserChoice {
  USER_CHOICE_UPDATE = 0,
  USER_CHOICE_SKIP = 1,
  USER_CHOICE_RESTART_ON_FAILURE = 2,
  USER_CHOICE_RESTART_ON_LOW_STORAGE = 3,
  USER_CHOICE_REPORT_AN_ISSUE = 4,
  USER_CHOICE_COUNT
};

// This enum must match the numbering for MigrationUIMigrationResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before COUNT.
enum class MigrationResult {
  SUCCESS_IN_NEW_MIGRATION = 0,
  SUCCESS_IN_RESUMED_MIGRATION = 1,
  GENERAL_FAILURE_IN_NEW_MIGRATION = 2,
  GENERAL_FAILURE_IN_RESUMED_MIGRATION = 3,
  REQUEST_FAILURE_IN_NEW_MIGRATION = 4,
  REQUEST_FAILURE_IN_RESUMED_MIGRATION = 5,
  MOUNT_FAILURE_IN_NEW_MIGRATION = 6,
  MOUNT_FAILURE_IN_RESUMED_MIGRATION = 7,
  COUNT
};

// This enum must match the numbering for MigrationUIRemoveCryptohomeResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before COUNT.
enum class RemoveCryptohomeResult {
  SUCCESS_IN_NEW_MIGRATION = 0,
  SUCCESS_IN_RESUMED_MIGRATION = 1,
  FAILURE_IN_NEW_MIGRATION = 2,
  FAILURE_IN_RESUMED_MIGRATION = 3,
  COUNT
};

bool IsTestingUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kTestEncryptionMigrationUI);
}

// Wrapper functions for histogram macros to avoid duplication of expanded code.
void RecordFirstScreen(FirstScreen first_screen) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameFirstScreen, first_screen,
                            FirstScreen::FIRST_SCREEN_COUNT);
}

void RecordUserChoice(UserChoice user_choice) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameUserChoice, user_choice,
                            UserChoice::USER_CHOICE_COUNT);
}

void RecordMigrationResult(MigrationResult migration_result) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameMigrationResult, migration_result,
                            MigrationResult::COUNT);
}

void RecordRemoveCryptohomeResult(bool success, bool is_resumed_migration) {
  RemoveCryptohomeResult result =
      success ? (is_resumed_migration
                     ? RemoveCryptohomeResult::SUCCESS_IN_RESUMED_MIGRATION
                     : RemoveCryptohomeResult::SUCCESS_IN_NEW_MIGRATION)
              : (is_resumed_migration
                     ? RemoveCryptohomeResult::FAILURE_IN_RESUMED_MIGRATION
                     : RemoveCryptohomeResult::FAILURE_IN_NEW_MIGRATION);
  UMA_HISTOGRAM_ENUMERATION(kUmaNameRemoveCryptohomeResult, result,
                            RemoveCryptohomeResult::COUNT);
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
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
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
  builder->Add("migrationButtonReportAnIssue", IDS_REPORT_AN_ISSUE);
}

void EncryptionMigrationScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

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
  AddCallback(
      kJsApiRequestRestartOnLowStorage,
      &EncryptionMigrationScreenHandler::HandleRequestRestartOnLowStorage);
  AddCallback(kJsApiRequestRestartOnFailure,
              &EncryptionMigrationScreenHandler::HandleRequestRestartOnFailure);
  AddCallback(kJsApiOpenFeedbackDialog,
              &EncryptionMigrationScreenHandler::HandleOpenFeedbackDialog);
}

void EncryptionMigrationScreenHandler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (proto.has_battery_percent()) {
    if (!current_battery_percent_) {
      // If initial battery level is below the minimum, migration should start
      // automatically once the device is charged enough.
      if (proto.battery_percent() < arc::kMigrationMinimumBatteryPercent)
        should_migrate_on_enough_battery_ = true;
    }
    current_battery_percent_ = proto.battery_percent();
  } else {
    // If battery level is not provided, we regard it as 100% to start migration
    // immediately.
    current_battery_percent_ = 100.0;
  }

  CallJS("setBatteryState", *current_battery_percent_,
         *current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent,
         proto.battery_state() ==
             power_manager::PowerSupplyProperties_BatteryState_CHARGING);

  // If the migration was already requested and the bettery level is enough now,
  // The migration should start immediately.
  if (*current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent &&
      should_migrate_on_enough_battery_) {
    should_migrate_on_enough_battery_ = false;
    StartMigration();
  }
}

void EncryptionMigrationScreenHandler::HandleStartMigration() {
  RecordUserChoice(UserChoice::USER_CHOICE_UPDATE);
  WaitBatteryAndMigrate();
}

void EncryptionMigrationScreenHandler::HandleSkipMigration() {
  RecordUserChoice(UserChoice::USER_CHOICE_SKIP);
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

void EncryptionMigrationScreenHandler::HandleRequestRestartOnLowStorage() {
  RecordUserChoice(UserChoice::USER_CHOICE_RESTART_ON_LOW_STORAGE);
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void EncryptionMigrationScreenHandler::HandleRequestRestartOnFailure() {
  RecordUserChoice(UserChoice::USER_CHOICE_RESTART_ON_FAILURE);
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void EncryptionMigrationScreenHandler::HandleOpenFeedbackDialog() {
  RecordUserChoice(UserChoice::USER_CHOICE_REPORT_AN_ISSUE);
  const std::string description = base::StringPrintf(
      "Auto generated feedback for http://crbug.com/719266.\n"
      "(uniquifier:%s)",
      base::Int64ToString(base::Time::Now().ToInternalValue()).c_str());
  login_feedback_.reset(new LoginFeedback(Profile::FromWebUI(web_ui())));
  login_feedback_->Request(description, base::Closure());
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

  // We should block power save and not shut down on lid close during migration.
  if (state == UIState::MIGRATING) {
    StartBlockingPowerSave();
    PowerPolicyController::Get()->SetEncryptionMigrationActive(true);
  } else {
    StopBlockingPowerSave();
    PowerPolicyController::Get()->SetEncryptionMigrationActive(false);
  }

  // Record which screen is visible to the user.
  // We record it after delay to make sure that the user was actually able
  // to see the screen (i.e. the screen is not just a flash).
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &EncryptionMigrationScreenHandler::OnDelayedRecordVisibleScreen,
          weak_ptr_factory_.GetWeakPtr(), state),
      base::TimeDelta::FromSeconds(1));
}

void EncryptionMigrationScreenHandler::CheckAvailableStorage() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&base::SysInfo::AmountOfFreeDiskSpace,
                 base::FilePath(kCheckStoragePath)),
      base::Bind(&EncryptionMigrationScreenHandler::OnGetAvailableStorage,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnGetAvailableStorage(int64_t size) {
  if (size >= arc::kMigrationMinimumAvailableStorage || IsTestingUI()) {
    if (should_resume_) {
      RecordFirstScreen(FirstScreen::FIRST_SCREEN_RESUME);
      WaitBatteryAndMigrate();
    } else {
      RecordFirstScreen(FirstScreen::FIRST_SCREEN_READY);
      UpdateUIState(UIState::READY);
    }
  } else {
    RecordFirstScreen(FirstScreen::FIRST_SCREEN_LOW_STORAGE);
    CallJS("setAvailableSpaceInString", ui::FormatBytes(size));
    CallJS("setNecessarySpaceInString",
           ui::FormatBytes(arc::kMigrationMinimumAvailableStorage));
    UpdateUIState(UIState::NOT_ENOUGH_STORAGE);
  }
}

void EncryptionMigrationScreenHandler::WaitBatteryAndMigrate() {
  if (current_battery_percent_ &&
      *current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent) {
    StartMigration();
    return;
  }
  UpdateUIState(UIState::READY);

  should_migrate_on_enough_battery_ = true;
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();
}

void EncryptionMigrationScreenHandler::StartMigration() {
  UpdateUIState(UIState::MIGRATING);
  initial_battery_percent_ = *current_battery_percent_;

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
    RecordMigrationResult(
        should_resume_ ? MigrationResult::MOUNT_FAILURE_IN_RESUMED_MIGRATION
                       : MigrationResult::MOUNT_FAILURE_IN_NEW_MIGRATION);
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

void EncryptionMigrationScreenHandler::StartBlockingPowerSave() {
  if (!power_save_blocker_) {
    power_save_blocker_.reset(new device::PowerSaveBlocker(
        device::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
        device::PowerSaveBlocker::kReasonOther,
        "Encryption migration is in progress...",
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::UI),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::FILE)));
  }
}

void EncryptionMigrationScreenHandler::StopBlockingPowerSave() {
  if (power_save_blocker_.get()) {
    power_save_blocker_.reset();
  }
}

void EncryptionMigrationScreenHandler::RemoveCryptohome() {
  // Set invalid token status so that user is forced to go through Gaia on the
  // next sign-in.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      user_context_.GetAccountId(),
      user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      cryptohome::Identification(user_context_.GetAccountId()),
      base::Bind(&EncryptionMigrationScreenHandler::OnRemoveCryptohome,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnRemoveCryptohome(
    bool success,
    cryptohome::MountError return_code) {
  LOG_IF(ERROR, !success) << "Removing cryptohome failed. return code: "
                          << return_code;
  RecordRemoveCryptohomeResult(success, should_resume_);
  UpdateUIState(UIState::MIGRATION_FAILED);
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
      RecordMigrationResult(should_resume_
                                ? MigrationResult::SUCCESS_IN_RESUMED_MIGRATION
                                : MigrationResult::SUCCESS_IN_NEW_MIGRATION);
      // If the battery level decreased during migration, record the consumed
      // battery level.
      if (*current_battery_percent_ < initial_battery_percent_) {
        UMA_HISTOGRAM_PERCENTAGE(
            kUmaNameConsumedBatteryPercent,
            static_cast<int>(std::round(initial_battery_percent_ -
                                        *current_battery_percent_)));
      }
      // Restart immediately after successful migration.
      DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_FAILED:
      RecordMigrationResult(
          should_resume_ ? MigrationResult::GENERAL_FAILURE_IN_RESUMED_MIGRATION
                         : MigrationResult::GENERAL_FAILURE_IN_NEW_MIGRATION);
      // Stop listening to the progress updates.
      DBusThreadManager::Get()
          ->GetCryptohomeClient()
          ->SetDircryptoMigrationProgressHandler(
              CryptohomeClient::DircryptoMigrationProgessHandler());
      // Shows error screen after removing user directory is completed.
      RemoveCryptohome();
      break;
    default:
      break;
  }
}

void EncryptionMigrationScreenHandler::OnMigrationRequested(bool success) {
  if (!success) {
    LOG(ERROR) << "Requesting MigrateToDircrypto failed.";
    RecordMigrationResult(
        should_resume_ ? MigrationResult::REQUEST_FAILURE_IN_RESUMED_MIGRATION
                       : MigrationResult::REQUEST_FAILURE_IN_NEW_MIGRATION);
    UpdateUIState(UIState::MIGRATION_FAILED);
  }
}

void EncryptionMigrationScreenHandler::OnDelayedRecordVisibleScreen(
    UIState ui_state) {
  if (current_ui_state_ != ui_state)
    return;

  // If |current_ui_state_| is not changed for a second, record the current
  // screen as a "visible" screen.
  UMA_HISTOGRAM_ENUMERATION(kUmaNameVisibleScreen, ui_state, UIState::COUNT);
}

}  // namespace chromeos
