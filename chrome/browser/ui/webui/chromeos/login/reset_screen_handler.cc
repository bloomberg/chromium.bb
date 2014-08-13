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
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

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
  if (reboot_was_requested_) {
    dialog_type = rollback_available_ ?
        reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_AND_ROLLBACK :
        reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_ONLY;
  } else {
    dialog_type = rollback_available_ ?
      reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_AVAILABLE :
      reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_UNAVAILABLE;
  }
  UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown",
                            dialog_type,
                            reset::DIALOG_VIEW_TYPE_SIZE);

  base::DictionaryValue reset_screen_params;
  reset_screen_params.SetBoolean("showRestartMsg", restart_required_);
  reset_screen_params.SetBoolean(
      "showRollbackOption", rollback_available_ && !reboot_was_requested_);
  reset_screen_params.SetBoolean(
      "simpleConfirm", reboot_was_requested_ && !rollback_available_);
  reset_screen_params.SetBoolean(
      "rollbackConfirm", reboot_was_requested_ && rollback_available_);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, false);
  prefs->SetBoolean(prefs::kRollbackRequested, false);
  prefs->CommitPendingWrite();
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
    PrefService* prefs = g_browser_process->local_state();
    rollback_available_ = prefs->GetBoolean(prefs::kRollbackRequested);
    ShowWithParams();
  } else {
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
        CanRollbackCheck(base::Bind(&ResetScreenHandler::OnRollbackCheck,
        weak_ptr_factory_.GetWeakPtr()));
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
  builder->Add("resetScreenIconTitle",IDS_RESET_SCREEN_ICON_TITLE);
  builder->Add("cancelButton", IDS_CANCEL);

  builder->Add("resetWarningDataDetails",
               IDS_RESET_SCREEN_WARNING_DETAILS_DATA);
  builder->Add("resetRestartMessage", IDS_RESET_SCREEN_RESTART_MSG);
  builder->AddF("resetRollbackOption",
                IDS_RESET_SCREEN_ROLLBACK_OPTION,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetRevertPromise",
                IDS_RESET_SCREEN_PREPARING_REVERT_PROMISE,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetRevertSpinnerMessage",
                IDS_RESET_SCREEN_PREPARING_REVERT_SPINNER_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);

  // Different variants of the same UI elements for all dialog cases.
  builder->Add("resetButtonReset", IDS_RESET_SCREEN_RESET);
  builder->Add("resetButtonRelaunch", IDS_RELAUNCH_BUTTON);
  builder->Add("resetButtonPowerwash", IDS_RESET_SCREEN_POWERWASH);

  builder->AddF(
      "resetAndRollbackWarningTextConfirmational",
      IDS_RESET_SCREEN_CONFIRMATION_WARNING_POWERWASH_AND_ROLLBACK_MSG,
      IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetWarningTextConfirmational",
                IDS_RESET_SCREEN_CONFIRMATION_WARNING_POWERWASH_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetWarningTextInitial",
                IDS_RESET_SCREEN_WARNING_MSG,
                IDS_SHORT_PRODUCT_NAME);

  builder->AddF(
      "resetAndRollbackWarningDetailsConfirmational",
      IDS_RESET_SCREEN_CONFIRMATION_WARNING_ROLLBACK_DETAILS,
      IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetWarningDetailsConfirmational",
                IDS_RESET_SCREEN_CONFIRMATION_WARNING_DETAILS,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetWarningDetailsInitial",
                IDS_RESET_SCREEN_WARNING_DETAILS,
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
  registry->RegisterBooleanPref(prefs::kRollbackRequested, false);
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
}

void ResetScreenHandler::HandleOnCancel() {
  if (preparing_for_rollback_)
    return;
  if (delegate_)
    delegate_->OnExit();
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void ResetScreenHandler::HandleOnRestart(bool should_rollback) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetBoolean(prefs::kRollbackRequested, should_rollback);
  prefs->CommitPendingWrite();

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void ResetScreenHandler::HandleOnPowerwash(bool rollback_checked) {
  if (rollback_available_ && (rollback_checked || reboot_was_requested_)) {
      preparing_for_rollback_ = true;
      CallJS("updateViewOnRollbackCall");
      DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->Rollback();
  } else {
    if (rollback_checked && !rollback_available_) {
      NOTREACHED() <<
          "Rollback was checked but not available. Starting powerwash.";
    }
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        StartDeviceWipe();
  }
}

void ResetScreenHandler::HandleOnLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_POWERWASH);
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
