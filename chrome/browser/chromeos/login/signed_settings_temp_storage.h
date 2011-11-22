// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_TEMP_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_TEMP_STORAGE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/values.h"

class PrefService;

namespace chromeos {

// There is need (proxy settings at OOBE stage) to store settings
// (that are normally go into SignedSettings storage)
// before owner has been assigned (hence no key is available).
// This class serves as a transient storage in that case.
class SignedSettingsTempStorage {
 public:
  // Registers required pref section.
  static void RegisterPrefs(PrefService* local_state);

  static bool Store(const std::string& name,
                    const base::Value& value,
                    PrefService* local_state);
  static bool Retrieve(const std::string& name,
                       base::Value** value,
                       PrefService* local_state);

  // Call this after owner has been assigned to persist settings
  // into SignedSettings storage.
  static void Finalize(PrefService* local_state);

 private:
  SignedSettingsTempStorage() {}
  DISALLOW_COPY_AND_ASSIGN(SignedSettingsTempStorage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNED_SETTINGS_TEMP_STORAGE_H_
