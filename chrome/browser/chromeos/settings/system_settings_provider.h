// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base {
class StringValue;
}

namespace chromeos {

class SystemSettingsProvider : public CrosSettingsProvider,
                               public system::TimezoneSettings::Observer {
 public:
  explicit SystemSettingsProvider(const NotifyObserversCallback& notify_cb);
  virtual ~SystemSettingsProvider();

  // CrosSettingsProvider implementation.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual TrustedStatus PrepareTrustedValues(
      const base::Closure& callback) OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;

  // TimezoneSettings::Observer implementation.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

 private:
  // CrosSettingsProvider implementation.
  virtual void DoSet(const std::string& path,
                     const base::Value& in_value) OVERRIDE;

  scoped_ptr<base::StringValue> timezone_value_;

  DISALLOW_COPY_AND_ASSIGN(SystemSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SYSTEM_SETTINGS_PROVIDER_H_
