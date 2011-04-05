// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_H_

#include <string>

class Value;

namespace chromeos {

class CrosSettingsProvider {
 public:
  virtual ~CrosSettingsProvider() {}

  // Sets |in_value| to given |path| in cros settings.
  // Note that this takes ownership of |in_value|.
  void Set(const std::string& path, Value* in_value);

  // Gets settings value of given |path| to |out_value|.
  // Note that |out_value| is owned by the caller, not this class.
  virtual bool Get(const std::string& path, Value** out_value) const = 0;

  // Gets the namespace prefix provided by this provider
  virtual bool HandlesSetting(const std::string& path) = 0;

 private:
  // Does the real job for Set().
  virtual void DoSet(const std::string& path, Value* in_value) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_PROVIDER_H_
