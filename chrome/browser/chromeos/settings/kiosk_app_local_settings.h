// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_KIOSK_APP_LOCAL_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_KIOSK_APP_LOCAL_SETTINGS_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"

namespace chromeos {

class KioskAppLocalSettings : public CrosSettingsProvider {
 public:
  explicit KioskAppLocalSettings(const NotifyObserversCallback& notify_cb);
  virtual ~KioskAppLocalSettings();

 private:
  // Reads "apps" dict from local state and populates |apps_|.
  void ReadApps();

  // Updates "apps" dict in local state based on |value|.
  void WriteApps(const base::Value& value);

  // Whether the given value contains an eligible app id for auto launch.
  // The app id in the value must either be an empty string or exists in
  // |apps_|.
  bool IsEligibleAutoLaunchAppId(const base::Value& value) const;

  // CrosSettingsProvider overrides:
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual TrustedStatus PrepareTrustedValues(
      const base::Closure& callback) OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;
  virtual void DoSet(const std::string& path,
                     const base::Value& in_value) OVERRIDE;

  // This needed because "apps" in local state could not be used direct as it
  // is a dictionary used for storing app info cache as well.
  base::ListValue apps_;

  base::StringValue default_auto_launch_;
  base::FundamentalValue default_disable_bailout_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppLocalSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_KIOSK_APP_LOCAL_SETTINGS_H_
