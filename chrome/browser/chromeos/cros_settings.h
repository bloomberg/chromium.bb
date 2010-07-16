// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_

#include <string>
#include "base/values.h"

#include "chrome/browser/chromeos/cros_settings_names.h"

namespace chromeos {

// A class manages per-device/global settings.
class CrosSettings {
 public:
  // Class factory.
  static CrosSettings* Get();

  // Helper function to test if given path is a value cros settings name.
  static bool IsCrosSettings(const std::wstring& path);

  // Sets |in_value| to given |path| in cros settings.
  // Note that this takes ownership of |in_value|.
  virtual void Set(const std::wstring& path, Value* in_value) = 0;

  // Gets settings value of given |path| to |out_value|.
  // Note that |out_value| is still owned by this class.
  virtual bool Get(const std::wstring& path, Value** out_value) const = 0;

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
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
