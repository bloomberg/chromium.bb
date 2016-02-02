// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/reset_screen.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/screens/reset_view.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/reset/metrics.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"


namespace chromeos {

ResetScreen::ResetScreen(BaseScreenDelegate* base_screen_delegate,
                         ResetView* view)
    : ResetModel(base_screen_delegate),
      view_(view),
      weak_ptr_factory_(this) {
  DCHECK(view_);
  if (view_)
    view_->Bind(*this);
  context_.SetInteger(kContextKeyScreenState, STATE_RESTART_REQUIRED);
  context_.SetBoolean(kContextKeyIsRollbackAvailable, false);
  context_.SetBoolean(kContextKeyIsRollbackChecked, false);
  context_.SetBoolean(kContextKeyIsConfirmational, false);
  context_.SetBoolean(kContextKeyIsOfficialBuild, false);
#if defined(OFFICIAL_BUILD)
  context_.SetBoolean(kContextKeyIsOfficialBuild, true);
#endif
}

ResetScreen::~ResetScreen() {
  if (view_)
    view_->Unbind();
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void ResetScreen::PrepareToShow() {
  if (view_)
    view_->PrepareToShow();
}

void ResetScreen::Show() {
  if (view_)
    view_->Show();

  int dialog_type = -1;  // used by UMA metrics.

  ContextEditor context_editor = GetContextEditor();

  bool restart_required = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFirstExecAfterBoot);
  if (restart_required) {
    context_editor.SetInteger(kContextKeyScreenState, STATE_RESTART_REQUIRED);
    dialog_type = reset::DIALOG_SHORTCUT_RESTART_REQUIRED;
  } else {
    context_editor.SetInteger(kContextKeyScreenState, STATE_POWERWASH_PROPOSAL);
  }

  // Set availability of Rollback feature.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRollbackOption)) {
    context_editor.SetBoolean(kContextKeyIsRollbackAvailable, false);
    dialog_type = reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_UNAVAILABLE;
  } else {
    chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
        CanRollbackCheck(base::Bind(&ResetScreen::OnRollbackCheck,
        weak_ptr_factory_.GetWeakPtr()));
  }

  if (dialog_type >= 0) {
    UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown",
                              dialog_type,
                              reset::DIALOG_VIEW_TYPE_SIZE);
  }

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, false);
  prefs->CommitPendingWrite();
}

void ResetScreen::Hide() {
  if (view_)
    view_->Hide();
}

void ResetScreen::OnViewDestroyed(ResetView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void ResetScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancelReset)
    OnCancel();
  else if (action_id == kUserActionResetRestartPressed)
    OnRestart();
  else if (action_id == kUserActionResetPowerwashPressed)
    OnPowerwash();
  else if (action_id == kUserActionResetLearnMorePressed)
    OnLearnMore();
  else if (action_id == kUserActionResetRollbackToggled)
    OnToggleRollback();
  else if (action_id == kUserActionResetShowConfirmationPressed)
    OnShowConfirm();
  else if (action_id == kUserActionResetResetConfirmationDismissed)
    OnConfirmationDismissed();
  else
    BaseScreen::OnUserAction(action_id);
}

void ResetScreen::OnCancel() {
  if (context_.GetInteger(
      kContextKeyScreenState, STATE_RESTART_REQUIRED) == STATE_REVERT_PROMISE)
    return;
  // Hide Rollback view for the next show.
  if (context_.GetBoolean(kContextKeyIsRollbackAvailable) &&
      context_.GetBoolean(kContextKeyIsRollbackChecked))
    OnToggleRollback();
  Finish(BaseScreenDelegate::RESET_CANCELED);
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
}

void ResetScreen::OnPowerwash() {
  if (context_.GetInteger(kContextKeyScreenState, 0) !=
      STATE_POWERWASH_PROPOSAL)
    return;

  GetContextEditor().SetBoolean(kContextKeyIsConfirmational, false);
  CommitContextChanges();

  if (context_.GetBoolean(kContextKeyIsRollbackAvailable) &&
      context_.GetBoolean(kContextKeyIsRollbackChecked)) {
    GetContextEditor().SetInteger(kContextKeyScreenState, STATE_REVERT_PROMISE);
    DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
    VLOG(1) << "Starting Rollback";
    DBusThreadManager::Get()->GetUpdateEngineClient()->Rollback();
  } else {
    if (context_.GetBoolean(kContextKeyIsRollbackChecked) &&
        !context_.GetBoolean(kContextKeyIsRollbackAvailable)) {
      NOTREACHED() <<
          "Rollback was checked but not available. Starting powerwash.";
    }
    VLOG(1) << "Starting Powerwash";
    DBusThreadManager::Get()->GetSessionManagerClient()->StartDeviceWipe();
  }
}

void ResetScreen::OnRestart() {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void ResetScreen::OnToggleRollback() {
  // Hide Rollback if visible.
  if (context_.GetBoolean(kContextKeyIsRollbackAvailable) &&
      context_.GetBoolean(kContextKeyIsRollbackChecked)) {
    VLOG(1) << "Hiding rollback view on reset screen";
    GetContextEditor().SetBoolean(kContextKeyIsRollbackChecked, false);
    return;
  }

  // Show Rollback if available.
  VLOG(1) << "Requested rollback availability" <<
             context_.GetBoolean(kContextKeyIsRollbackAvailable);
  if (context_.GetBoolean(kContextKeyIsRollbackAvailable) &&
      !context_.GetBoolean(kContextKeyIsRollbackChecked)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Reset.ChromeOS.PowerwashDialogShown",
        reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_AVAILABLE,
        reset::DIALOG_VIEW_TYPE_SIZE);
    GetContextEditor().SetBoolean(kContextKeyIsRollbackChecked, true);
  }
}

void ResetScreen::OnShowConfirm() {
  int dialog_type = context_.GetBoolean(kContextKeyIsRollbackChecked) ?
      reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_AND_ROLLBACK :
      reset::DIALOG_SHORTCUT_CONFIRMING_POWERWASH_ONLY;
  UMA_HISTOGRAM_ENUMERATION(
      "Reset.ChromeOS.PowerwashDialogShown",
      dialog_type,
      reset::DIALOG_VIEW_TYPE_SIZE);

  GetContextEditor().SetBoolean(kContextKeyIsConfirmational, true);
}

void ResetScreen::OnLearnMore() {
#if defined(OFFICIAL_BUILD)
  VLOG(1) << "Trying to view the help article about reset options.";
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_POWERWASH);
#endif
}

void ResetScreen::OnConfirmationDismissed() {
  GetContextEditor().SetBoolean(kContextKeyIsConfirmational, false);
}

void ResetScreen::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  VLOG(1) << "Update status change to " << status.status;
  if (status.status == UpdateEngineClient::UPDATE_STATUS_ERROR ||
      status.status ==
          UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT) {
    GetContextEditor().SetInteger(kContextKeyScreenState, STATE_ERROR);
    // Show error screen.
    GetErrorScreen()->SetUIState(NetworkError::UI_STATE_ROLLBACK_ERROR);
    get_base_screen_delegate()->ShowErrorScreen();
  } else if (status.status ==
      UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  }
}

// Invoked from call to CanRollbackCheck upon completion of the DBus call.
void ResetScreen::OnRollbackCheck(bool can_rollback) {
  VLOG(1) << "Callback from CanRollbackCheck, result " << can_rollback;
  int dialog_type = can_rollback ?
      reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_AVAILABLE :
      reset::DIALOG_SHORTCUT_OFFERING_ROLLBACK_UNAVAILABLE;
  UMA_HISTOGRAM_ENUMERATION("Reset.ChromeOS.PowerwashDialogShown",
                            dialog_type,
                            reset::DIALOG_VIEW_TYPE_SIZE);

  GetContextEditor().SetBoolean(kContextKeyIsRollbackAvailable, can_rollback);
}

ErrorScreen* ResetScreen::GetErrorScreen() {
  return get_base_screen_delegate()->GetErrorScreen();
}

}  // namespace chromeos
