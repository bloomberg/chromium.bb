// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_SYSTEM_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_SYSTEM_SETTINGS_PROVIDER_H_

#include <vector>

#include "base/callback.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "third_party/icu/public/i18n/unicode/timezone.h"

namespace base {
class Value;
class ListValue;
class StringValue;
}

namespace chromeos {

class SystemSettingsProvider : public CrosSettingsProvider,
                               public system::TimezoneSettings::Observer {
 public:
  SystemSettingsProvider();
  virtual ~SystemSettingsProvider();

  // CrosSettingsProvider overrides.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual bool GetTrusted(const std::string& path,
                          const base::Closure& callback) const OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;

  // Overridden from TimezoneSettings::Observer:
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

  // Creates the map of timezones used by the options page.
  base::ListValue* GetTimezoneList();

 private:
  // CrosSettingsProvider overrides.
  virtual void DoSet(const std::string& path,
                     const base::Value& in_value) OVERRIDE;

  // Gets timezone name.
  static string16 GetTimezoneName(const icu::TimeZone& timezone);

  // Gets timezone ID which is also used as timezone pref value.
  static string16 GetTimezoneID(const icu::TimeZone& timezone);

  // Gets timezone object from its id.
  const icu::TimeZone* GetTimezone(const string16& timezone_id);

  // Gets a timezone id from a timezone in |timezones_| that has the same
  // rule of given |timezone|.
  // One timezone could have multiple timezones,
  // e.g.
  //   US/Pacific == America/Los_Angeles
  // We should always use the known timezone id when passing back as
  // pref values.
  string16 GetKnownTimezoneID(const icu::TimeZone& timezone) const;

  // Timezones.
  std::vector<icu::TimeZone*> timezones_;
  // TODO(pastarmovj): This will be cached in the local_state PrefStore soon.
  mutable scoped_ptr<base::StringValue> system_timezone_;

  DISALLOW_COPY_AND_ASSIGN(SystemSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_SYSTEM_SETTINGS_PROVIDER_H_
