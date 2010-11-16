// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"

class ListValue;
class PrefService;

namespace chromeos {

class UserCrosSettingsProvider : public CrosSettingsProvider,
                                 public SignedSettingsHelper::Callback {
 public:
  UserCrosSettingsProvider();
  virtual ~UserCrosSettingsProvider();

  // Registers cached users settings in preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Helper functions to access cached settings.
  static bool cached_allow_guest();
  static bool cached_allow_new_user();
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

  // SignedSettingsHelper::Callback overrides.
  virtual void OnWhitelistCompleted(bool success, const std::string& email);
  virtual void OnUnwhitelistCompleted(bool success, const std::string& email);
  virtual void OnStorePropertyCompleted(
      bool success, const std::string& name, const std::string& value);
  virtual void OnRetrievePropertyCompleted(
      bool success, const std::string& name, const std::string& value);

  void WhitelistUser(const std::string& email);
  void UnwhitelistUser(const std::string& email);

  // Updates cached value of the owner.
  static void UpdateCachedOwner(const std::string& email);

 private:
  // CrosSettingsProvider implementation.
  virtual void DoSet(const std::string& path, Value* value);

  void StartFetchingBoolSetting(const std::string& name);
  void StartFetchingStringSetting(const std::string& name);
  void StartFetchingSetting(const std::string& name);

  base::hash_set<std::string> bool_settings_;
  base::hash_set<std::string> string_settings_;

  DISALLOW_COPY_AND_ASSIGN(UserCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
