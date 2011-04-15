// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"

class ListValue;
class PrefService;
class Task;

namespace chromeos {

// CrosSettingsProvider implementation that works with SignedSettings.
// TODO(nkostylev): Rename this class to indicate that it is
// SignedSettings specific.
class UserCrosSettingsProvider : public CrosSettingsProvider {
 public:
  UserCrosSettingsProvider();
  virtual ~UserCrosSettingsProvider() {}

  // Registers cached users settings in preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Methods to use when trusted (signature verified) values are required.
  // Return true if subsequent call to corresponding cached_* getter shall
  // return trusted value.
  // Return false if trusted values are unavailable at a moment.
  // In latter case passed task will be posted when ready.
  bool RequestTrustedAllowGuest(Task* callback);
  bool RequestTrustedAllowNewUser(Task* callback);
  bool RequestTrustedDataRoamingEnabled(Task* callback);
  bool RequestTrustedShowUsersOnSignin(Task* callback);
  bool RequestTrustedOwner(Task* callback);

  // Reloads values from device settings.
  void Reload();

  // Helper functions to access cached settings.
  static bool cached_allow_guest();
  static bool cached_allow_new_user();
  static bool cached_data_roaming_enabled();
  static bool cached_show_users_on_signin();
  static const ListValue* cached_whitelist();
  static std::string cached_owner();

  // Returns true if given email is in user whitelist.
  // Note this function is for display purpose only and should use
  // CheckWhitelist op for the real whitelist check.
  static bool IsEmailInCachedWhitelist(const std::string& email);

  // CrosSettingsProvider implementation.
  virtual bool Get(const std::string& path, Value** out_value) const;
  virtual bool HandlesSetting(const std::string& path);

  void WhitelistUser(const std::string& email);
  void UnwhitelistUser(const std::string& email);

  // Updates cached value of the owner.
  static void UpdateCachedOwner(const std::string& email);

 private:
  // CrosSettingsProvider implementation.
  virtual void DoSet(const std::string& path, Value* value);

  DISALLOW_COPY_AND_ASSIGN(UserCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
