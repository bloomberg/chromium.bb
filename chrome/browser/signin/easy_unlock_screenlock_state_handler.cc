// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/chromeos_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

size_t kIconSize = 27u;
size_t kSpinnerResourceWidth = 1215u;
size_t kSpinnerIntervalMs = 50u;

const char kHardlockIconResourceURL[] =
    "chrome://theme/IDR_EASY_UNLOCK_HARDLOCKED";

const char kLockedIconResourceURL[] =
    "chrome://theme/IDR_EASY_UNLOCK_LOCKED";

const char kSpinnerIconResourceURL[] =
    "chrome://theme/IDR_EASY_UNLOCK_SPINNER";

const char kUnlockedIconResourceURL[] =
    "chrome://theme/IDR_EASY_UNLOCK_UNLOCKED";

std::string GetIconURLForState(EasyUnlockScreenlockStateHandler::State state) {
  switch (state) {
    case EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH:
    case EasyUnlockScreenlockStateHandler::STATE_NO_PHONE:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_NEARBY:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE:
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED:
      return kLockedIconResourceURL;
    case EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING:
      return kSpinnerIconResourceURL;
    case EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED:
      return kUnlockedIconResourceURL;
    default:
      return "";
  }
}

bool HasAnimation(EasyUnlockScreenlockStateHandler::State state) {
  return state == EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING;
}

bool HardlockOnClick(EasyUnlockScreenlockStateHandler::State state) {
  return state != EasyUnlockScreenlockStateHandler::STATE_INACTIVE;
}

size_t GetTooltipResourceId(EasyUnlockScreenlockStateHandler::State state) {
  switch (state) {
    case EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_BLUETOOTH;
    case EasyUnlockScreenlockStateHandler::STATE_NO_PHONE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_NO_PHONE;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_NOT_AUTHENTICATED;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_LOCKED;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_UNLOCKABLE;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_NEARBY:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_PHONE_NOT_NEARBY;
    case EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK_INSTRUCTIONS;
    case EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED:
      return IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_UNSUPPORTED_ANDROID_VERSION;
    default:
      return 0;
  }
}

bool TooltipContainsDeviceType(EasyUnlockScreenlockStateHandler::State state) {
  return state == EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED ||
         state == EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE ||
         state == EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH ||
         state == EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED;
}

}  // namespace


EasyUnlockScreenlockStateHandler::EasyUnlockScreenlockStateHandler(
    const std::string& user_email,
    bool initially_hardlocked,
    PrefService* pref_service,
    ScreenlockBridge* screenlock_bridge)
    : state_(STATE_INACTIVE),
      user_email_(user_email),
      pref_service_(pref_service),
      screenlock_bridge_(screenlock_bridge),
      hardlocked_(initially_hardlocked),
      hardlock_ui_shown_(false) {
  DCHECK(screenlock_bridge_);
  screenlock_bridge_->AddObserver(this);
}

EasyUnlockScreenlockStateHandler::~EasyUnlockScreenlockStateHandler() {
  screenlock_bridge_->RemoveObserver(this);
  // Make sure the screenlock state set by this gets cleared.
  ChangeState(STATE_INACTIVE);
}

void EasyUnlockScreenlockStateHandler::ChangeState(State new_state) {
  if (state_ == new_state)
    return;

  state_ = new_state;

  // If lock screen is not active or it forces offline password, just cache the
  // current state. The screenlock state will get refreshed in |ScreenDidLock|.
  if (!screenlock_bridge_->IsLocked())
    return;

  if (hardlocked_) {
    ShowHardlockUI();
    return;
  }

  UpdateScreenlockAuthType();

  ScreenlockBridge::UserPodCustomIconOptions icon_options;

  std::string icon_url = GetIconURLForState(state_);
  if (icon_url.empty()) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(user_email_);
    return;
  }
  icon_options.SetIconAsResourceURL(icon_url);

  bool trial_run = IsTrialRun();

  UpdateTooltipOptions(trial_run, &icon_options);

  icon_options.SetSize(kIconSize, kIconSize);

  if (HasAnimation(state_))
    icon_options.SetAnimation(kSpinnerResourceWidth, kSpinnerIntervalMs);

  // Don't hardlock on trial run.
  if (!trial_run && HardlockOnClick(state_))
    icon_options.SetHardlockOnClick();

  if (trial_run && state_ == STATE_AUTHENTICATED)
    MarkTrialRunComplete();

  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(user_email_,
                                                            icon_options);
}

void EasyUnlockScreenlockStateHandler::SetHardlocked(bool value) {
  if (hardlocked_ == value)
    return;

  hardlocked_ = value;

  // If hardlocked_ was set to false, this means the screen is about to get
  // unlocked. No need to update it in this case.
  if (hardlocked_) {
    State last_state = state_;
    // This should force updating screenlock state.
    state_ = STATE_INACTIVE;
    ChangeState(last_state);
  }
}

void EasyUnlockScreenlockStateHandler::OnScreenDidLock() {
  State last_state = state_;
  // This should force updating screenlock state.
  state_ = STATE_INACTIVE;
  ChangeState(last_state);
}

void EasyUnlockScreenlockStateHandler::OnScreenDidUnlock() {
  hardlock_ui_shown_ = false;
  if (state_ != STATE_INACTIVE)
    MarkTrialRunComplete();
}

void EasyUnlockScreenlockStateHandler::OnFocusedUserChanged(
    const std::string& user_id) {
}

void EasyUnlockScreenlockStateHandler::ShowHardlockUI() {
  DCHECK(hardlocked_);

  if (!screenlock_bridge_->IsLocked())
    return;

  if (screenlock_bridge_->lock_handler()->GetAuthType(user_email_) !=
          ScreenlockBridge::LockHandler::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
        base::string16());
  }

  if (state_ == STATE_INACTIVE) {
    screenlock_bridge_->lock_handler()->HideUserPodCustomIcon(user_email_);
    hardlock_ui_shown_ = false;
    return;
  }

  if (hardlock_ui_shown_)
    return;

  ScreenlockBridge::UserPodCustomIconOptions icon_options;
  icon_options.SetIconAsResourceURL(kHardlockIconResourceURL);
  icon_options.SetSize(kIconSize, kIconSize);
  icon_options.SetTooltip(
      l10n_util::GetStringFUTF16(IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_HARDLOCK,
                                 GetDeviceName()),
      false /* don't autoshow */);
  screenlock_bridge_->lock_handler()->ShowUserPodCustomIcon(user_email_,
                                                            icon_options);
  hardlock_ui_shown_ = true;
}

void EasyUnlockScreenlockStateHandler::UpdateTooltipOptions(
    bool trial_run,
    ScreenlockBridge::UserPodCustomIconOptions* icon_options) {
  size_t resource_id = 0;
  base::string16 device_name;
  if (trial_run && state_ == STATE_AUTHENTICATED) {
    resource_id = IDS_EASY_UNLOCK_SCREENLOCK_TOOLTIP_TUTORIAL;
  } else {
    resource_id = GetTooltipResourceId(state_);
    if (TooltipContainsDeviceType(state_))
      device_name = GetDeviceName();
  }

  if (!resource_id)
    return;

  base::string16 tooltip;
  if (device_name.empty()) {
    tooltip = l10n_util::GetStringUTF16(resource_id);
  } else {
    tooltip = l10n_util::GetStringFUTF16(resource_id, device_name);
  }

  if (tooltip.empty())
    return;

  icon_options->SetTooltip(
      tooltip,
      state_ == STATE_AUTHENTICATED && trial_run /* autoshow tooltip */);
}

bool EasyUnlockScreenlockStateHandler::IsTrialRun() {
  return pref_service_ &&
         pref_service_->GetBoolean(prefs::kEasyUnlockShowTutorial);
}

void EasyUnlockScreenlockStateHandler::MarkTrialRunComplete() {
  if (!pref_service_)
    return;
  pref_service_->SetBoolean(prefs::kEasyUnlockShowTutorial, false);
}

base::string16 EasyUnlockScreenlockStateHandler::GetDeviceName() {
#if defined(OS_CHROMEOS)
  return chromeos::GetChromeDeviceType();
#else
  // TODO(tbarzic): Figure out the name for non Chrome OS case.
  return base::ASCIIToUTF16("Chrome");
#endif
}

void EasyUnlockScreenlockStateHandler::UpdateScreenlockAuthType() {
  if (hardlocked_)
    return;

  if (state_ == STATE_AUTHENTICATED) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::USER_CLICK,
        l10n_util::GetStringUTF16(
            IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE));
  } else if (screenlock_bridge_->lock_handler()->GetAuthType(user_email_) !=
                 ScreenlockBridge::LockHandler::OFFLINE_PASSWORD) {
    screenlock_bridge_->lock_handler()->SetAuthType(
        user_email_,
        ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
        base::string16());
  }
}
