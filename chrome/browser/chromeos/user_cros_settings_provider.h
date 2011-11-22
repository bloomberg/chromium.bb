// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"

class PrefService;

namespace base {
class ListValue;
}

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

  // Reloads values from device settings.
  void Reload();

  // CrosSettingsProvider implementation.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual bool GetTrusted(const std::string& path,
                          const base::Closure& callback) const OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;

  static void WhitelistUser(const std::string& email);
  static void UnwhitelistUser(const std::string& email);

  // Updates cached value of the owner.
  static void UpdateCachedOwner(const std::string& email);

 private:
  // CrosSettingsProvider implementation.
  virtual void DoSet(const std::string& path,
                     const base::Value& value) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UserCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_USER_CROS_SETTINGS_PROVIDER_H_
