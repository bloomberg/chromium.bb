// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

const char kJsScreenPath[] = "login.ResetScreen";

// Reset screen id.
const char kResetScreen[] = "reset";

}  // namespace

namespace chromeos {

ResetScreenHandler::ResetScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false),
      restart_required_(true),
      reboot_was_requested_(false),
      rollback_available_(false),
      weak_factory_(this) {
}

ResetScreenHandler::~ResetScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void ResetScreenHandler::PrepareToShow() {
}

void ResetScreenHandler::ShowWithParams() {
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

  PrefService* prefs = g_browser_process->local_state();
  restart_required_ = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFirstExecAfterBoot);
  reboot_was_requested_ = false;
  rollback_available_ = false;
  if (!restart_required_)  // First exec after boot.
    reboot_was_requested_ = prefs->GetBoolean(prefs::kFactoryResetRequested);
  if (!restart_required_ && reboot_was_requested_) {
    // First exec after boot.
    rollback_available_ = prefs->GetBoolean(prefs::kRollbackRequested);
    ShowWithParams();
  } else {
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
        CanRollbackCheck(base::Bind(&ResetScreenHandler::OnRollbackCheck,
        weak_factory_.GetWeakPtr()));
  }
}

void ResetScreenHandler::Hide() {
}

void ResetScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void ResetScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("resetScreenTitle", IDS_RESET_SCREEN_TITLE);
  builder->Add("cancelButton", IDS_CANCEL);

  builder->Add("resetWarningDataDetails",
               IDS_RESET_SCREEN_WARNING_DETAILS_DATA);
  builder->Add("resetRestartMessage", IDS_RESET_SCREEN_RESTART_MSG);
  builder->AddF("resetRollbackOption",
                IDS_RESET_SCREEN_ROLLBACK_OPTION,
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
  if (delegate_)
    delegate_->OnExit();
}

void ResetScreenHandler::HandleOnRestart(bool should_rollback) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->SetBoolean(prefs::kRollbackRequested, should_rollback);
  prefs->CommitPendingWrite();

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void ResetScreenHandler::HandleOnPowerwash() {
  if (rollback_available_) {
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->Rollback();
  } else {
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        StartDeviceWipe();
  }
}

void ResetScreenHandler::HandleOnLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_POWERWASH);
}

}  // namespace chromeos
