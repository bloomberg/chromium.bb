// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/reset/metrics.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kJsScreenPath[] = "login.ResetScreen";

// Reset screen id.
const char kResetScreen[] = "reset";

const int kErrorUIStateRollback = 7;

}  // namespace

namespace chromeos {

ResetScreenHandler::ResetScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false),
      restart_required_(true),
      reboot_was_requested_(false),
      rollback_available_(false),
      rollback_checked_(false),
      preparing_for_rollback_(false),
      weak_ptr_factory_(this) {
}

ResetScreenHandler::~ResetScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void ResetScreenHandler::PrepareToShow() {
}

void ResetScreenHandler::ShowWithParams() {
  int dialog_type;
  if (restart_required_) {
    dialog_type = reset::DIALOG_SHORTCUT_RESTART_REQUIRED;
  } else {
    dialog_type = reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_UNAVAILABLE;
  }
  UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown",
                            dialog_type,
                            reset::DIALOG_VIEW_TYPE_SIZE);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, false);
  prefs->CommitPendingWrite();
  base::DictionaryValue reset_screen_params;
  reset_screen_params.SetBoolean("restartRequired", restart_required_);
  reset_screen_params.SetBoolean("rollbackAvailable", rollback_available_);
  ShowScreen(kResetScreen, &reset_screen_params);
}

void ResetScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  ChooseAndApplyShowScenario();
}

void ResetScreenHandler::ChooseAndApplyShowScenario() {
  PrefService* prefs = g_browser_process->local_state();
  restart_required_ = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFirstExecAfterBoot);

  reboot_was_requested_ = false;
  rollback_available_ = false;
  preparing_for_rollback_ = false;
  if (!restart_required_)  // First exec after boot.
    reboot_was_requested_ = prefs->GetBoolean(prefs::kFactoryResetRequested);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableRollbackOption)) {
    rollback_available_ = false;
    ShowWithParams();
  } else if (!restart_required_ && reboot_was_requested_) {
    // First exec after boot.
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
        CanRollbackCheck(base::Bind(&ResetScreenHandler::OnRollbackCheck,
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Will require restart.
    ShowWithParams();
  }
}

void ResetScreenHandler::Hide() {
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void ResetScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void ResetScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("resetScreenTitle", IDS_RESET_SCREEN_TITLE);
  builder->Add("resetScreenAccessibleTitle", IDS_RESET_SCREEN_TITLE);
  builder->Add("resetScreenIconTitle", IDS_RESET_SCREEN_ICON_TITLE);
  builder->Add("cancelButton", IDS_CANCEL);
  builder->Add("resetButtonReset", IDS_RESET_SCREEN_RESET);
  builder->Add("resetButtonRestart", IDS_RELAUNCH_BUTTON);
  builder->Add("resetButtonPowerwash", IDS_RESET_SCREEN_POWERWASH);
  builder->Add("resetButtonPowerwashAndRollback",
               IDS_RESET_SCREEN_POWERWASH_AND_REVERT);

  builder->Add("resetWarningDataDetails",
               IDS_RESET_SCREEN_WARNING_DETAILS_DATA);
  builder->Add("resetRestartMessage", IDS_RESET_SCREEN_RESTART_MSG);
  builder->AddF("resetRevertPromise",
                IDS_RESET_SCREEN_PREPARING_REVERT_PROMISE,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetRevertSpinnerMessage",
                IDS_RESET_SCREEN_PREPARING_REVERT_SPINNER_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);

  // Variants for screen title.
  builder->AddF("resetWarningTitle",
                IDS_RESET_SCREEN_WARNING_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetPowerwashWarningTitle",
                IDS_RESET_SCREEN_WARNING_POWERWASH_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetPowerwasAndRollbackhWarningTitle",
                IDS_RESET_SCREEN_WARNING_POWERWASH_AND_ROLLBACK_MSG,
                IDS_SHORT_PRODUCT_NAME);

  // Variants for screen message.
  builder->AddF("resetPowerwashWarningDetails",
                IDS_RESET_SCREEN_WARNING_DETAILS,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetPowerwashRollbackWarningDetails",
                IDS_RESET_SCREEN_WARNING_POWERWASH_AND_ROLLBACK_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetPowerwashConfirmationDetails",
                IDS_RESET_SCREEN_CONFIRMATION_WARNING_DETAILS,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetPowerwashRollbackConfirmationDetails",
                IDS_RESET_SCREEN_CONFIRMATION_WARNING_ROLLBACK_DETAILS,
                IDS_SHORT_PRODUCT_NAME);
}

// Invoked from call to CanRollbackCheck upon completion of the DBus call.
void ResetScreenHandler::OnRollbackCheck(bool can_rollback) {
  VLOG(1) << "Callback from CanRollbackCheck, result " << can_rollback;
  rollback_available_ = can_rollback;
  ShowWithParams();
}

// static
void ResetScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kFactoryResetRequested, false);
}

void ResetScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void ResetScreenHandler::RegisterMessages() {
  AddCallback("cancelOnReset", &ResetScreenHandler::HandleOnCancel);
  AddCallback("restartOnReset", &ResetScreenHandler::HandleOnRestart);
  AddCallback("powerwashOnReset", &ResetScreenHandler::HandleOnPowerwash);
  AddCallback("resetOnLearnMore", &ResetScreenHandler::HandleOnLearnMore);
  AddCallback(
      "showRollbackOnResetScreen", &ResetScreenHandler::HandleOnShowRollback);
  AddCallback(
      "hideRollbackOnResetScreen", &ResetScreenHandler::HandleOnHideRollback);
  AddCallback(
      "showConfirmationOnReset", &ResetScreenHandler::HandleOnShowConfirm);
}

void ResetScreenHandler::HandleOnCancel() {
  if (preparing_for_rollback_)
    return;
  if (delegate_)
    delegate_->OnExit();
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void ResetScreenHandler::HandleOnRestart() {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void ResetScreenHandler::HandleOnPowerwash(bool rollback_checked) {
  if (rollback_available_ && rollback_checked) {
      preparing_for_rollback_ = true;
      CallJS("updateViewOnRollbackCall");
      DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
      VLOG(1) << "Starting Rollback";
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->Rollback();
  } else {
    if (rollback_checked && !rollback_available_) {
      NOTREACHED() <<
          "Rollback was checked but not available. Starting powerwash.";
    }
    VLOG(1) << "Starting Powerwash";
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        StartDeviceWipe();
  }
}

void ResetScreenHandler::HandleOnLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_POWERWASH);
}

void ResetScreenHandler::HandleOnShowRollback() {
  VLOG(1) << "Requested rollback availability" << rollback_available_;
  if (rollback_available_) {
    UMA_HISTOGRAM_ENUMERATION(
        "Reset.ChromeOS.PowerwashDialogShown",
        reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_AVAILABLE,
        reset::DIALOG_VIEW_TYPE_SIZE);
    CallJS("showRollbackOption");
    rollback_checked_ = true;
  }
}

void ResetScreenHandler::HandleOnHideRollback() {
  if (rollback_available_ && rollback_checked_) {
    CallJS("hideRollbackOption");
    rollback_checked_ = false;
  }
}

void ResetScreenHandler::HandleOnShowConfirm() {
  int dialog_type = rollback_checked_ ?
      reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_AND_ROLLBACK :
      reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_ONLY;
  UMA_HISTOGRAM_ENUMERATION(
      "Reset.ChromeOS.PowerwashDialogShown",
      dialog_type,
      reset::DIALOG_VIEW_TYPE_SIZE);
}

void ResetScreenHandler::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  VLOG(1) << "Update status change to " << status.status;
  if (status.status == UpdateEngineClient::UPDATE_STATUS_ERROR ||
      status.status ==
          UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    preparing_for_rollback_ = false;
    // Show error screen.
    base::DictionaryValue params;
    params.SetInteger("uiState", kErrorUIStateRollback);
    ShowScreen(OobeUI::kScreenErrorMessage, &params);
  } else if (status.status ==
      UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  }
}

}  // namespace chromeos
