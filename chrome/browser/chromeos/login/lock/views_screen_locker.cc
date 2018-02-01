// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/views_screen_locker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/user_selection_screen_proxy.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
constexpr char kLockDisplay[] = "lock";
}  // namespace

ViewsScreenLocker::ViewsScreenLocker(ScreenLocker* screen_locker)
    : screen_locker_(screen_locker),
      version_info_updater_(this),
      weak_factory_(this) {
  LoginScreenClient::Get()->SetDelegate(this);
  user_selection_screen_proxy_ = std::make_unique<UserSelectionScreenProxy>();
  user_selection_screen_ =
      std::make_unique<ChromeUserSelectionScreen>(kLockDisplay);
  user_selection_screen_->SetView(user_selection_screen_proxy_.get());

  allowed_input_methods_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceLoginScreenInputMethods,
          base::Bind(&ViewsScreenLocker::OnAllowedInputMethodsChanged,
                     base::Unretained(this)));
}

ViewsScreenLocker::~ViewsScreenLocker() {
  if (lock_screen_apps::StateController::IsEnabled())
    lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(nullptr);
  LoginScreenClient::Get()->SetDelegate(nullptr);
}

void ViewsScreenLocker::Init() {
  lock_time_ = base::TimeTicks::Now();
  user_selection_screen_->Init(screen_locker_->users());
  LoginScreenClient::Get()->LoadUsers(
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

  version_info::Channel channel = chrome::GetChannel();
  bool should_show_version = (channel == version_info::Channel::STABLE ||
                              channel == version_info::Channel::BETA)
                                 ? false
                                 : true;
  if (should_show_version) {
#if defined(OFFICIAL_BUILD)
    version_info_updater_.StartUpdate(true);
#else
    version_info_updater_.StartUpdate(false);
#endif
  }
}

void ViewsScreenLocker::OnLockScreenReady() {
  lock_screen_ready_ = true;
  user_selection_screen_->InitEasyUnlock();
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  screen_locker_->ScreenLockReady();
  if (lock_screen_apps::StateController::IsEnabled())
    lock_screen_apps::StateController::Get()->SetFocusCyclerDelegate(this);
  OnAllowedInputMethodsChanged();
}

void ViewsScreenLocker::SetPasswordInputEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void ViewsScreenLocker::ShowErrorMessage(
    int error_msg_id,
    HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(xiaoyinh): Complete the implementation here.
  LoginScreenClient::Get()->ShowErrorMessage(0 /* login_attempts */,
                                             std::string(), std::string(),
                                             static_cast<int>(help_topic_id));
}

void ViewsScreenLocker::ClearErrors() {
  LoginScreenClient::Get()->ClearErrors();
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
  // Notify session controller that the lock animations are done.
  // This is used to notify chromeos::PowerEventObserver that lock screen UI
  // has finished showing. PowerEventObserver uses this notification during
  // device suspend - device suspend is delayed until lock UI reports it's done
  // animating. Additionally, PowerEventObserver will not stop root windows
  // compositors until it receives this notification.
  // Historically, this was called when Web UI lock implementation reported
  // that all animations for showing the UI have finished, which gave enough
  // time to update display's frame buffers with new UI before compositing was
  // stopped.
  // This is not the case with views lock implementation.
  // OnAshLockAnimationFinished() is called too soon, thus the display's frame
  // buffers might still contain the UI from before the lock window was shown
  // at this time - see https://crbug.com/807511.
  // To work around this, add additional delay before notifying
  // PowerEventObserver lock screen UI is ready.
  // TODO(tbarzic): Find a more deterministic way to determine when the display
  //     can be turned off during device suspend.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ViewsScreenLocker::NotifyChromeLockAnimationsComplete,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(1500));
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
    const password_manager::SyncPasswordData& sync_password_data,
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
  Key::KeyType key_type =
      authenticated_by_pin ? chromeos::Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234
                           : chromeos::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF;
  user_context.SetKey(Key(key_type, std::string(), hashed_password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetSyncPasswordData(sync_password_data);
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
    WallpaperManager::Get()->ShowUserWallpaper(account_id);

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

bool ViewsScreenLocker::HandleFocusLockScreenApps(bool reverse) {
  if (lock_screen_app_focus_handler_.is_null())
    return false;

  lock_screen_app_focus_handler_.Run(reverse);
  return true;
}

void ViewsScreenLocker::HandleLoginAsGuest() {
  NOTREACHED();
}

void ViewsScreenLocker::SuspendDone(const base::TimeDelta& sleep_duration) {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUnlockUsers()) {
    UpdatePinKeyboardState(user->GetAccountId());
  }
}

void ViewsScreenLocker::RegisterLockScreenAppFocusHandler(
    const LockScreenAppFocusCallback& focus_handler) {
  lock_screen_app_focus_handler_ = focus_handler;
}

void ViewsScreenLocker::UnregisterLockScreenAppFocusHandler() {
  lock_screen_app_focus_handler_.Reset();
}

void ViewsScreenLocker::HandleLockScreenAppFocusOut(bool reverse) {
  LoginScreenClient::Get()->HandleFocusLeavingLockScreenApps(reverse);
}

void ViewsScreenLocker::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  os_version_label_text_ = os_version_label_text;
  OnDevChannelInfoUpdated();
}

void ViewsScreenLocker::OnEnterpriseInfoUpdated(const std::string& message_text,
                                                const std::string& asset_id) {
  if (asset_id.empty())
    return;
  enterprise_info_text_ = l10n_util::GetStringFUTF8(
      IDS_OOBE_ASSET_ID_LABEL, base::UTF8ToUTF16(asset_id));
  OnDevChannelInfoUpdated();
}

void ViewsScreenLocker::OnDeviceInfoUpdated(const std::string& bluetooth_name) {
  bluetooth_name_ = bluetooth_name;
  OnDevChannelInfoUpdated();
}

void ViewsScreenLocker::NotifyChromeLockAnimationsComplete() {
  SessionControllerClient::Get()->NotifyChromeLockAnimationsComplete();
}

void ViewsScreenLocker::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  if (!quick_unlock_storage)
    return;

  bool is_enabled = quick_unlock_storage->IsPinAuthenticationAvailable();
  LoginScreenClient::Get()->SetPinEnabledForUser(account_id, is_enabled);
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

void ViewsScreenLocker::OnDevChannelInfoUpdated() {
  LoginScreenClient::Get()->SetDevChannelInfo(
      os_version_label_text_, enterprise_info_text_, bluetooth_name_);
}

}  // namespace chromeos
