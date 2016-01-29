// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_ARC_SETTINGS_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SETTINGS_BRIDGE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/values.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

namespace fontsizes {

// The following values were obtained from chrome://settings and Android's
// Display settings on Nov 2015. They are expected to remain stable.
const float kAndroidFontScaleSmall = 0.85;
const float kAndroidFontScaleNormal = 1;
const float kAndroidFontScaleLarge = 1.15;
const float kAndroidFontScaleHuge = 1.3;
const int kChromeFontSizeNormal = 16;
const int kChromeFontSizeLarge = 20;
const int kChromeFontSizeVeryLarge = 24;

// Android has only a single float value for system-wide font size
// (font_scale).  Chrome has three main int pixel values that affect
// system-wide font size.  We will take the largest font value of the three
// main font values on Chrome and convert to an Android size.
double ConvertFontSizeChromeToAndroid(int default_size,
                                      int default_fixed_size,
                                      int minimum_size);

}  // namespace fontsizes

// Listens to changes for select Chrome settings (prefs) that Android cares
// about and sends the new values to Android to keep the state in sync.
class SettingsBridge : public chromeos::system::TimezoneSettings::Observer {
 public:
  class Delegate {
   public:
    virtual void OnBroadcastNeeded(const std::string& action,
                                   const base::DictionaryValue& extras) = 0;
  };

  explicit SettingsBridge(Delegate* delegate);
  ~SettingsBridge() override;

  // Called when a Chrome pref we have registered an observer for has changed.
  // Obtains the new pref value and sends it to Android.
  void OnPrefChanged(const std::string& pref_name) const;

  // TimezoneSettings::Observer
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  // Registers to observe changes for Chrome settings we care about.
  void StartObservingSettingsChanges();

  // Stops listening for Chrome settings changes.
  void StopObservingSettingsChanges();

  // Retrives Chrome's state for the settings and send it to Android.
  void SyncAllPrefs() const;
  void SyncFontSize() const;
  void SyncLocale() const;
  void SyncSpokenFeedbackEnabled() const;
  void SyncTimeZone() const;
  void SyncUse24HourClock() const;

  // Registers to listen to a particular perf.
  void AddPrefToObserve(const std::string& pref_name);

  // Returns the integer value of the pref.  pref_name must exist.
  int GetIntegerPref(const std::string& pref_name) const;

  // Sends a broadcast to the delegate.
  void SendSettingsBroadcast(const std::string& action,
                             const base::DictionaryValue& extras) const;

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;

  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(SettingsBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_SETTINGS_BRIDGE_H_
