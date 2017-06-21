// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/views_screen_locker.h"

#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/user_selection_screen_proxy.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {

namespace {
constexpr char kLockDisplay[] = "lock";
}  // namespace

ViewsScreenLocker::ViewsScreenLocker(ScreenLocker* screen_locker)
    : screen_locker_(screen_locker), weak_factory_(this) {
  LockScreenClient::Get()->SetDelegate(this);
  user_selection_screen_proxy_ = base::MakeUnique<UserSelectionScreenProxy>();
  user_selection_screen_ =
      base::MakeUnique<ChromeUserSelectionScreen>(kLockDisplay);
  user_selection_screen_->SetView(user_selection_screen_proxy_.get());

  allowed_input_methods_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceLoginScreenInputMethods,
          base::Bind(&ViewsScreenLocker::OnAllowedInputMethodsChanged,
                     base::Unretained(this)));
}

ViewsScreenLocker::~ViewsScreenLocker() {
  LockScreenClient::Get()->SetDelegate(nullptr);
}

void ViewsScreenLocker::Init() {
  lock_time_ = base::TimeTicks::Now();
  user_selection_screen_->Init(screen_locker_->users());
  LockScreenClient::Get()->LoadUsers(
      user_selection_screen_->UpdateAndReturnUserListForMojo(),
      false /* show_guests */);
  if (!ime_state_.get())
    ime_state_ = input_method::InputMethodManager::Get()->GetActiveIMEState();

  // Reset Caps Lock state when lock screen is shown.
  input_method::InputMethodManager::Get()->GetImeKeyboard()->SetCapsLockEnabled(
      false);

  // Enable pin for any users who can use it.
  if (user_manager::UserManager::IsInitialized()) {
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetLoggedInUsers()) {
      UpdatePinKeyboardState(user->GetAccountId());
    }
  }
}

void ViewsScreenLocker::OnLockScreenReady() {
  lock_screen_ready_ = true;
  user_selection_screen_->InitEasyUnlock();
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  screen_locker_->ScreenLockReady();
  OnAllowedInputMethodsChanged();
}

void ViewsScreenLocker::SetPasswordInputEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::ShowErrorMessage(
    int error_msg_id,
    HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(xiaoyinh): Complete the implementation here.
  LockScreenClient::Get()->ShowErrorMessage(0 /* login_attempts */,
                                            std::string(), std::string(),
                                            static_cast<int>(help_topic_id));
}

void ViewsScreenLocker::ClearErrors() {
  LockScreenClient::Get()->ClearErrors();
}

void ViewsScreenLocker::AnimateAuthenticationSuccess() {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::OnLockWebUIReady() {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::OnLockBackgroundDisplayed() {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::OnHeaderBarVisible() {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::OnAshLockAnimationFinished() {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::SetFingerprintState(
    const AccountId& account_id,
    ScreenLocker::FingerprintState state) {
  NOTIMPLEMENTED();
}

content::WebContents* ViewsScreenLocker::GetWebContents() {
  return nullptr;
}

void ViewsScreenLocker::HandleAuthenticateUser(
    const AccountId& account_id,
    const std::string& hashed_password,
    bool authenticated_by_pin,
    AuthenticateUserCallback callback) {
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  // If pin storage is unavailable, |authenticated_by_pin| must be false.
  DCHECK(!quick_unlock_storage ||
         quick_unlock_storage->IsPinAuthenticationAvailable() ||
         !authenticated_by_pin);

  UserContext user_context(account_id);
  user_context.SetKey(Key(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, std::string(),
                          hashed_password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    user_context.SetUserType(user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  ScreenLocker::default_screen_locker()->Authenticate(user_context,
                                                      std::move(callback));
  UpdatePinKeyboardState(account_id);
}

void ViewsScreenLocker::HandleAttemptUnlock(const AccountId& account_id) {
  user_selection_screen_->AttemptEasyUnlock(account_id);
}

void ViewsScreenLocker::HandleHardlockPod(const AccountId& account_id) {
  user_selection_screen_->HardLockPod(account_id);
}

void ViewsScreenLocker::HandleRecordClickOnLockIcon(
    const AccountId& account_id) {
  user_selection_screen_->RecordClickOnLockIcon(account_id);
}

void ViewsScreenLocker::HandleOnFocusPod(const AccountId& account_id) {
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id);
  if (user_selection_screen_)
    user_selection_screen_->CheckUserStatus(account_id);

  focused_pod_account_id_ = base::Optional<AccountId>(account_id);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // |user| may be null in kiosk mode or unit tests.
  if (user && user->is_logged_in() && !user->is_active()) {
    SessionControllerClient::DoSwitchActiveUser(account_id);
  } else {
    lock_screen_utils::SetUserInputMethod(account_id.GetUserEmail(),
                                          ime_state_.get());
    lock_screen_utils::SetKeyboardSettings(account_id);
    WallpaperManager::Get()->SetUserWallpaperDelayed(account_id);

    bool use_24hour_clock = false;
    if (user_manager::known_user::GetBooleanPref(
            account_id, prefs::kUse24HourClock, &use_24hour_clock)) {
      g_browser_process->platform_part()
          ->GetSystemClock()
          ->SetLastFocusedPodHourClockType(
              use_24hour_clock ? base::k24HourClock : base::k12HourClock);
    }
  }
}

void ViewsScreenLocker::HandleOnNoPodFocused() {
  focused_pod_account_id_.reset();
  lock_screen_utils::EnforcePolicyInputMethods(std::string());
}

void ViewsScreenLocker::SuspendDone(const base::TimeDelta& sleep_duration) {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUnlockUsers()) {
    UpdatePinKeyboardState(user->GetAccountId());
  }
}

void ViewsScreenLocker::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  if (!quick_unlock_storage)
    return;

  bool is_enabled = quick_unlock_storage->IsPinAuthenticationAvailable();
  LockScreenClient::Get()->SetPinEnabledForUser(account_id, is_enabled);
}

void ViewsScreenLocker::OnAllowedInputMethodsChanged() {
  if (!lock_screen_ready_)
    return;

  if (focused_pod_account_id_) {
    std::string user_input_method = lock_screen_utils::GetUserLastInputMethod(
        focused_pod_account_id_->GetUserEmail());
    lock_screen_utils::EnforcePolicyInputMethods(user_input_method);
  } else {
    lock_screen_utils::EnforcePolicyInputMethods(std::string());
  }
}

}  // namespace chromeos
