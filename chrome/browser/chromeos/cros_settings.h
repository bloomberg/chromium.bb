// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#pragma once

#include <string>
#include <vector>
#include "base/singleton.h"
#include "base/values.h"

#include "chrome/browser/chromeos/cros_settings_names.h"

namespace chromeos {

class CrosSettingsProvider;

// A class manages per-device/global settings.
class CrosSettings {
 public:
  // Class factory.
  static CrosSettings* Get();

  // Helper function to test if given path is a value cros settings name.
  static bool IsCrosSettings(const std::wstring& path);

  // Sets |in_value| to given |path| in cros settings.
  // Note that this takes ownership of |in_value|.
  void Set(const std::wstring& path, Value* in_value);

  // Gets settings value of given |path| to |out_value|.
  // Note that |out_value| is still owned by this class.
  bool Get(const std::wstring& path, Value** out_value) const;

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  void SetBoolean(const std::wstring& path, bool in_value);
  void SetInteger(const std::wstring& path, int in_value);
  void SetReal(const std::wstring& path, double in_value);
  void SetString(const std::wstring& path, const std::string& in_value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(const std::wstring& path, bool* out_value) const;
  bool GetInteger(const std::wstring& path, int* out_value) const;
  bool GetReal(const std::wstring& path, double* out_value) const;
  bool GetString(const std::wstring& path, std::string* out_value) const;

 private:
  // adding/removing of providers
  bool AddProvider(CrosSettingsProvider* provider);
  bool RemoveProvider(CrosSettingsProvider* provider);

  std::vector<CrosSettingsProvider*> providers_;

  CrosSettings();
  ~CrosSettings();
  CrosSettingsProvider* GetProvider(const std::wstring& path) const;
  friend struct DefaultSingletonTraits<CrosSettings>;
  DISALLOW_COPY_AND_ASSIGN(CrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
