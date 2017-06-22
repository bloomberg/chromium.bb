// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "services/service_manager/public/cpp/connector.h"

using ash::mojom::TrayActionState;

namespace lock_screen_apps {

namespace {

StateController* g_instance = nullptr;

}  // namespace

// static
bool StateController::IsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableLockScreenApps);
}

// static
StateController* StateController::Get() {
  DCHECK(g_instance || !IsEnabled());
  return g_instance;
}

StateController::StateController()
    : binding_(this), app_window_observer_(this), session_observer_(this) {
  DCHECK(!g_instance);
  DCHECK(IsEnabled());

  g_instance = this;
}

StateController::~StateController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void StateController::SetTrayActionPtrForTesting(
    ash::mojom::TrayActionPtr tray_action_ptr) {
  tray_action_ptr_ = std::move(tray_action_ptr);
}

void StateController::FlushTrayActionForTesting() {
  tray_action_ptr_.FlushForTesting();
}

void StateController::SetAppManagerForTesting(
    std::unique_ptr<AppManager> app_manager) {
  DCHECK(!app_manager_);
  app_manager_ = std::move(app_manager);
}

void StateController::Initialize() {
  // The tray action ptr might be set previously if the client was being created
  // for testing.
  if (!tray_action_ptr_) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(ash::mojom::kServiceName, &tray_action_ptr_);
  }
  ash::mojom::TrayActionClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  tray_action_ptr_->SetClient(std::move(client), lock_screen_note_state_);
}

void StateController::SetPrimaryProfile(Profile* profile) {
  // App manager might have been set previously by a test.
  if (!app_manager_)
    app_manager_ = base::MakeUnique<AppManagerImpl>();
  app_manager_->Initialize(
      profile,
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile());

  session_observer_.Add(session_manager::SessionManager::Get());
  OnSessionStateChanged();
}

void StateController::AddObserver(StateObserver* observer) {
  observers_.AddObserver(observer);
}

void StateController::RemoveObserver(StateObserver* observer) {
  observers_.RemoveObserver(observer);
}

TrayActionState StateController::GetLockScreenNoteState() const {
  return lock_screen_note_state_;
}

void StateController::RequestNewLockScreenNote() {
  if (lock_screen_note_state_ != TrayActionState::kAvailable)
    return;

  DCHECK(app_manager_->IsNoteTakingAppAvailable());

  // Update state to launching even if app fails to launch - this is to notify
  // listeners that a lock screen note request was handled.
  UpdateLockScreenNoteState(TrayActionState::kLaunching);
  if (!app_manager_->LaunchNoteTaking())
    UpdateLockScreenNoteState(TrayActionState::kAvailable);
}

void StateController::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    app_manager_->Stop();
    ResetNoteTakingWindowAndMoveToNextState(true /* close_window */);
    return;
  }

  // base::Unretained is safe here because |app_manager_| is owned by |this|,
  // and the callback will not be invoked after |app_manager_| goes out of
  // scope.
  app_manager_->Start(
      base::Bind(&StateController::OnNoteTakingAvailabilityChanged,
                 base::Unretained(this)));
  OnNoteTakingAvailabilityChanged();
}

void StateController::OnAppWindowRemoved(extensions::AppWindow* app_window) {
  if (note_app_window_ != app_window)
    return;
  ResetNoteTakingWindowAndMoveToNextState(false /* close_window */);
}

extensions::AppWindow* StateController::CreateAppWindowForLockScreenAction(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::api::app_runtime::ActionType action,
    std::unique_ptr<extensions::AppDelegate> app_delegate) {
  if (action != extensions::api::app_runtime::ACTION_TYPE_NEW_NOTE)
    return nullptr;

  if (lock_screen_note_state_ != TrayActionState::kLaunching)
    return nullptr;

  if (!chromeos::ProfileHelper::GetSigninProfile()->IsSameProfile(
          Profile::FromBrowserContext(context))) {
    return nullptr;
  }

  if (!extension || app_manager_->GetNoteTakingAppId() != extension->id())
    return nullptr;

  // The ownership of the window is passed to the caller of this method.
  note_app_window_ =
      new extensions::AppWindow(context, app_delegate.release(), extension);
  app_window_observer_.Add(extensions::AppWindowRegistry::Get(
      chromeos::ProfileHelper::GetSigninProfile()));
  UpdateLockScreenNoteState(TrayActionState::kActive);
  return note_app_window_;
}

void StateController::MoveToBackground() {
  if (GetLockScreenNoteState() != TrayActionState::kActive)
    return;
  UpdateLockScreenNoteState(TrayActionState::kBackground);
}

void StateController::MoveToForeground() {
  if (GetLockScreenNoteState() != TrayActionState::kBackground)
    return;
  UpdateLockScreenNoteState(TrayActionState::kActive);
}

void StateController::OnNoteTakingAvailabilityChanged() {
  if (!app_manager_->IsNoteTakingAppAvailable() ||
      (note_app_window_ && note_app_window_->GetExtension()->id() !=
                               app_manager_->GetNoteTakingAppId())) {
    ResetNoteTakingWindowAndMoveToNextState(true /* close_window */);
    return;
  }

  if (GetLockScreenNoteState() == TrayActionState::kNotAvailable)
    UpdateLockScreenNoteState(TrayActionState::kAvailable);
}

void StateController::ResetNoteTakingWindowAndMoveToNextState(
    bool close_window) {
  app_window_observer_.RemoveAll();

  if (note_app_window_) {
    if (close_window && note_app_window_->GetBaseWindow())
      note_app_window_->GetBaseWindow()->Close();
    note_app_window_ = nullptr;
  }

  UpdateLockScreenNoteState(app_manager_->IsNoteTakingAppAvailable()
                                ? TrayActionState::kAvailable
                                : TrayActionState::kNotAvailable);
}

bool StateController::UpdateLockScreenNoteState(TrayActionState state) {
  const TrayActionState old_state = GetLockScreenNoteState();
  if (old_state == state)
    return false;

  lock_screen_note_state_ = state;
  NotifyLockScreenNoteStateChanged();
  return true;
}

void StateController::NotifyLockScreenNoteStateChanged() {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(lock_screen_note_state_);

  tray_action_ptr_->UpdateLockScreenNoteState(lock_screen_note_state_);
}

}  // namespace lock_screen_apps
