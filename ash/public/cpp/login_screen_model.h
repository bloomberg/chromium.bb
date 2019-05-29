// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_SCREEN_MODEL_H_
#define ASH_PUBLIC_CPP_LOGIN_SCREEN_MODEL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string16.h"

class AccountId;

namespace ash {

enum class FingerprintState;
enum class OobeDialogState;
struct EasyUnlockIconOptions;
struct InputMethodItem;
struct LocaleItem;
struct LoginUserInfo;
struct UserAvatar;

// Provides Chrome access to Ash's login UI. See additional docs for
// ash::LoginDataDispatcher.
class ASH_PUBLIC_EXPORT LoginScreenModel {
 public:
  // Requests to show the custom icon in the user pod.
  // |account_id|:  The account id of the user in the user pod.
  // |icon|:        Information regarding the icon.
  virtual void ShowEasyUnlockIcon(const AccountId& account_id,
                                  const EasyUnlockIconOptions& icon) = 0;

  // Shows a warning banner message on the login screen. A warning banner is
  // used to notify users of important messages before they log in to their
  // session. (e.g. Tell the user that an update of the user data will start
  // on login.) If |message| is empty, the banner will be hidden.
  virtual void UpdateWarningMessage(const base::string16& message) = 0;

  // Set the users who are displayed on the login UI. |users| is filtered
  // and does not correspond to every user on the device.
  virtual void SetUserList(const std::vector<LoginUserInfo>& users) = 0;

  // Update the status of fingerprint for |account_id|.
  virtual void SetFingerprintState(const AccountId& account_id,
                                   FingerprintState state) = 0;

  // Called when |avatar| for |account_id| has changed.
  virtual void SetAvatarForUser(const AccountId& account_id,
                                const UserAvatar& avatar) = 0;

  // Set the public session display name for user with |account_id|.
  virtual void SetPublicSessionDisplayName(const AccountId& account_id,
                                           const std::string& display_name) = 0;

  // Set the public session locales for user with |account_id|.
  // |locales|:            Available locales for this user.
  // |default_locale|:     Default locale for this user.
  // |show_advanced_view|: True if we should show the advanced expanded user
  //                       view for the public session.
  virtual void SetPublicSessionLocales(const AccountId& account_id,
                                       const std::vector<LocaleItem>& locales,
                                       const std::string& default_locale,
                                       bool show_advanced_view) = 0;

  // Set the public session keyboard layouts for user with |account_id|.
  // |locale|: The locale that |keyboard_layouts| can be used for.
  virtual void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      const std::vector<InputMethodItem>& keyboard_layouts) = 0;

  // Sets whether full management disclosure is needed for the public/managed
  // session login screen.
  virtual void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) = 0;

  // Called when focus is reported to be leaving a lock screen app window.
  // Requests focus to be handed off to the next suitable widget.
  // |reverse|:   Whether the tab order is reversed.
  virtual void HandleFocusLeavingLockScreenApps(bool reverse) = 0;

  // Called when the dialog hosting oobe has changed state. The oobe dialog
  // provides support for any part of login that is implemented in JS/HTML, such
  // as add user or powerwash.
  virtual void NotifyOobeDialogState(OobeDialogState state) = 0;

 protected:
  virtual ~LoginScreenModel();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_SCREEN_MODEL_H_
