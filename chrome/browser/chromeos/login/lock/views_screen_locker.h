// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/ui/ash/lock_screen_client.h"

namespace chromeos {

class UserSelectionScreen;
class UserSelectionScreenProxy;

// ViewsScreenLocker acts like LockScreenClient::Delegate which handles method
// calls coming from ash into chrome.
// It is also a ScreenLocker::Delegate which handles calls from chrome into
// ash (views-based lockscreen).
class ViewsScreenLocker : public LockScreenClient::Delegate,
                          public ScreenLocker::Delegate {
 public:
  explicit ViewsScreenLocker(ScreenLocker* screen_locker);
  ~ViewsScreenLocker() override;

  // ScreenLocker::Delegate:
  void SetPasswordInputEnabled(bool enabled) override;
  void ShowErrorMessage(int error_msg_id,
                        HelpAppLauncher::HelpTopic help_topic_id) override;
  void ClearErrors() override;
  void AnimateAuthenticationSuccess() override;
  void OnLockWebUIReady() override;
  void OnLockBackgroundDisplayed() override;
  void OnHeaderBarVisible() override;
  void OnAshLockAnimationFinished() override;
  void SetFingerprintState(const AccountId& account_id,
                           ScreenLocker::FingerprintState state) override;
  content::WebContents* GetWebContents() override;

  void Init();
  void OnLockScreenReady();

 private:
  // LockScreenClient::Delegate
  void HandleAttemptUnlock(const AccountId& account_id) override;
  void HandleHardlockPod(const AccountId& account_id) override;
  void HandleRecordClickOnLockIcon(const AccountId& account_id) override;

  std::unique_ptr<UserSelectionScreenProxy> user_selection_screen_proxy_;
  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  // The ScreenLocker that owns this instance.
  ScreenLocker* const screen_locker_ = nullptr;

  // Time when lock was initiated, required for metrics.
  base::TimeTicks lock_time_;

  base::WeakPtrFactory<ViewsScreenLocker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewsScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOCK_VIEWS_SCREEN_LOCKER_H_
