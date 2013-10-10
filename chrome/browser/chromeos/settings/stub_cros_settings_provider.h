// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_CROS_SETTINGS_PROVIDER_H_

#include <string>

#include "base/prefs/pref_value_map.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace chromeos {

class CrosSettings;

// CrosSettingsProvider implementation that stores settings in memory unsigned.
class StubCrosSettingsProvider : public CrosSettingsProvider {
 public:
  explicit StubCrosSettingsProvider(const NotifyObserversCallback& notify_cb);
  StubCrosSettingsProvider();
  virtual ~StubCrosSettingsProvider();

  // CrosSettingsProvider implementation.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual TrustedStatus PrepareTrustedValues(
      const base::Closure& callback) OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;

 private:
  // CrosSettingsProvider implementation:
  virtual void DoSet(const std::string& path,
                     const base::Value& value) OVERRIDE;

  // Initializes settings to their defaults.
  void SetDefaults();

  // In-memory settings storage.
  PrefValueMap values_;

  CrosSettings* cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(StubCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_STUB_CROS_SETTINGS_PROVIDER_H_
