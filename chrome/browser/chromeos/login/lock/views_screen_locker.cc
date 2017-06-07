// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/views_screen_locker.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/user_selection_screen_proxy.h"

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
}

ViewsScreenLocker::~ViewsScreenLocker() {
  LockScreenClient::Get()->SetDelegate(nullptr);
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

void ViewsScreenLocker::Init() {
  lock_time_ = base::TimeTicks::Now();
  user_selection_screen_->Init(screen_locker_->users());
  LockScreenClient::Get()->LoadUsers(user_selection_screen_->PrepareUserList(),
                                     false /* show_guests */);
}

void ViewsScreenLocker::OnLockScreenReady() {
  user_selection_screen_->InitEasyUnlock();
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  screen_locker_->ScreenLockReady();
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

}  // namespace chromeos
