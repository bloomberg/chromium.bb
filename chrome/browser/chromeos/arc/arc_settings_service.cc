// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_settings_service.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/arc/intent_helper/font_size_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

using ::chromeos::CrosSettings;
using ::chromeos::system::TimezoneSettings;

namespace arc {

// Listens to changes for select Chrome settings (prefs) that Android cares
// about and sends the new values to Android to keep the state in sync.
class ArcSettingsServiceImpl
    : public chromeos::system::TimezoneSettings::Observer {
 public:
  explicit ArcSettingsServiceImpl(ArcBridgeService* arc_bridge_service);
  ~ArcSettingsServiceImpl() override;

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
  void SyncReportingConsent() const;
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

  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      reporting_consent_subscription_;
  ArcBridgeService* const arc_bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsServiceImpl);
};

ArcSettingsServiceImpl::ArcSettingsServiceImpl(
    ArcBridgeService* arc_bridge_service)
    : arc_bridge_service_(arc_bridge_service) {
  StartObservingSettingsChanges();
  SyncAllPrefs();
}

ArcSettingsServiceImpl::~ArcSettingsServiceImpl() {
  StopObservingSettingsChanges();
}

void ArcSettingsServiceImpl::StartObservingSettingsChanges() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  registrar_.Init(profile->GetPrefs());

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize);
  AddPrefToObserve(prefs::kAccessibilitySpokenFeedbackEnabled);
  AddPrefToObserve(prefs::kUse24HourClock);

  reporting_consent_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      chromeos::kStatsReportingPref,
      base::Bind(&ArcSettingsServiceImpl::SyncReportingConsent,
                 base::Unretained(this)));

  TimezoneSettings::GetInstance()->AddObserver(this);
}

void ArcSettingsServiceImpl::SyncAllPrefs() const {
  SyncFontSize();
  SyncLocale();
  SyncReportingConsent();
  SyncSpokenFeedbackEnabled();
  SyncTimeZone();
  SyncUse24HourClock();
}

void ArcSettingsServiceImpl::StopObservingSettingsChanges() {
  registrar_.RemoveAll();
  reporting_consent_subscription_.reset();

  TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void ArcSettingsServiceImpl::AddPrefToObserve(const std::string& pref_name) {
  registrar_.Add(pref_name, base::Bind(&ArcSettingsServiceImpl::OnPrefChanged,
                                       base::Unretained(this)));
}

void ArcSettingsServiceImpl::OnPrefChanged(const std::string& pref_name) const {
  if (pref_name == prefs::kAccessibilitySpokenFeedbackEnabled) {
    SyncSpokenFeedbackEnabled();
  } else if (pref_name == prefs::kWebKitDefaultFixedFontSize ||
             pref_name == prefs::kWebKitDefaultFontSize ||
             pref_name == prefs::kWebKitMinimumFontSize) {
    SyncFontSize();
  } else if (pref_name == prefs::kUse24HourClock) {
    SyncUse24HourClock();
  } else {
    LOG(ERROR) << "Unknown pref changed.";
  }
}

void ArcSettingsServiceImpl::TimezoneChanged(const icu::TimeZone& timezone) {
  SyncTimeZone();
}

int ArcSettingsServiceImpl::GetIntegerPref(const std::string& pref_name) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  int val = -1;
  bool value_exists = pref->GetValue()->GetAsInteger(&val);
  DCHECK(value_exists);
  return val;
}

void ArcSettingsServiceImpl::SyncFontSize() const {
  int default_size = GetIntegerPref(prefs::kWebKitDefaultFontSize);
  int default_fixed_size = GetIntegerPref(prefs::kWebKitDefaultFixedFontSize);
  int minimum_size = GetIntegerPref(prefs::kWebKitMinimumFontSize);

  double android_scale = ConvertFontSizeChromeToAndroid(
      default_size, default_fixed_size, minimum_size);

  base::DictionaryValue extras;
  extras.SetDouble("scale", android_scale);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_FONT_SCALE",
                        extras);
}

void ArcSettingsServiceImpl::SyncSpokenFeedbackEnabled() const {
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      prefs::kAccessibilitySpokenFeedbackEnabled);
  DCHECK(pref);
  bool enabled = false;
  bool value_exists = pref->GetValue()->GetAsBoolean(&enabled);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetBoolean("enabled", enabled);
  SendSettingsBroadcast(
      "org.chromium.arc.intent_helper.SET_SPOKEN_FEEDBACK_ENABLED", extras);
}

void ArcSettingsServiceImpl::SyncLocale() const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(prefs::kApplicationLocale);
  DCHECK(pref);
  std::string locale;
  bool value_exists = pref->GetValue()->GetAsString(&locale);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetString("locale", locale);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_LOCALE", extras);
}

void ArcSettingsServiceImpl::SyncReportingConsent() const {
  bool consent = false;
  CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref, &consent);
  base::DictionaryValue extras;
  extras.SetBoolean("reportingConsent", consent);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_REPORTING_CONSENT",
                        extras);
}

void ArcSettingsServiceImpl::SyncTimeZone() const {
  TimezoneSettings* timezone_settings = TimezoneSettings::GetInstance();
  base::string16 timezoneID = timezone_settings->GetCurrentTimezoneID();
  base::DictionaryValue extras;
  extras.SetString("olsonTimeZone", timezoneID);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_TIME_ZONE", extras);
}

void ArcSettingsServiceImpl::SyncUse24HourClock() const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(prefs::kUse24HourClock);
  DCHECK(pref);
  bool use24HourClock = false;
  bool value_exists = pref->GetValue()->GetAsBoolean(&use24HourClock);
  DCHECK(value_exists);
  base::DictionaryValue extras;
  extras.SetBoolean("use24HourClock", use24HourClock);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_USE_24_HOUR_CLOCK",
                        extras);
}

void ArcSettingsServiceImpl::SendSettingsBroadcast(
    const std::string& action,
    const base::DictionaryValue& extras) const {
  if (!arc_bridge_service_->intent_helper_instance()) {
    LOG(ERROR) << "IntentHelper instance is not ready.";
    return;
  }

  std::string extras_json;
  bool write_success = base::JSONWriter::Write(extras, &extras_json);
  DCHECK(write_success);

  if (arc_bridge_service_->intent_helper_version() >= 1) {
    arc_bridge_service_->intent_helper_instance()->SendBroadcast(
        action, "org.chromium.arc.intent_helper",
        "org.chromium.arc.intent_helper.SettingsReceiver", extras_json);
  }
}

ArcSettingsService::ArcSettingsService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {
  arc_bridge_service()->AddObserver(this);
}

ArcSettingsService::~ArcSettingsService() {
  arc_bridge_service()->RemoveObserver(this);
}

void ArcSettingsService::OnIntentHelperInstanceReady() {
  impl_.reset(new ArcSettingsServiceImpl(arc_bridge_service()));
}

void ArcSettingsService::OnIntentHelperInstanceClosed() {
  impl_.reset();
}

}  // namespace arc
