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
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "net/proxy/proxy_config.h"

using ::chromeos::CrosSettings;
using ::chromeos::system::TimezoneSettings;

namespace {

bool GetHttpProxyServer(const ProxyConfigDictionary& proxy_config_dict,
                        std::string* host,
                        int* port) {
  std::string proxy_rules_string;
  if (!proxy_config_dict.GetProxyServer(&proxy_rules_string))
    return false;

  net::ProxyConfig::ProxyRules proxy_rules;
  proxy_rules.ParseFromString(proxy_rules_string);

  const net::ProxyList* proxy_list = nullptr;
  if (proxy_rules.type == net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY) {
    proxy_list = &proxy_rules.single_proxies;
  } else if (proxy_rules.type ==
             net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME) {
    proxy_list = proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);
  }
  if (!proxy_list || proxy_list->IsEmpty())
    return false;

  const net::ProxyServer& server = proxy_list->Get();
  *host = server.host_port_pair().host();
  *port = server.host_port_pair().port();
  return !host->empty() && *port;
}

}  // namespace

namespace arc {

// Listens to changes for select Chrome settings (prefs) that Android cares
// about and sends the new values to Android to keep the state in sync.
class ArcSettingsServiceImpl
    : public chromeos::system::TimezoneSettings::Observer,
      public device::BluetoothAdapter::Observer {
 public:
  explicit ArcSettingsServiceImpl(ArcBridgeService* arc_bridge_service);
  ~ArcSettingsServiceImpl() override;

  // Called when a Chrome pref we have registered an observer for has changed.
  // Obtains the new pref value and sends it to Android.
  void OnPrefChanged(const std::string& pref_name) const;

  // TimezoneSettings::Observer
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // BluetoothAdapter::Observer
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

 private:
  // Registers to observe changes for Chrome settings we care about.
  void StartObservingSettingsChanges();

  // Stops listening for Chrome settings changes.
  void StopObservingSettingsChanges();

  // Retrives Chrome's state for the settings and send it to Android.
  void SyncAllPrefs() const;
  void SyncFontSize() const;
  void SyncLocale() const;
  void SyncProxySettings() const;
  void SyncReportingConsent() const;
  void SyncSpokenFeedbackEnabled() const;
  void SyncTimeZone() const;
  void SyncUse24HourClock() const;

  void OnBluetoothAdapterInitialized(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Registers to listen to a particular perf.
  void AddPrefToObserve(const std::string& pref_name);

  // Returns the integer value of the pref.  pref_name must exist.
  int GetIntegerPref(const std::string& pref_name) const;

  // Sends a broadcast to the delegate.
  void SendSettingsBroadcast(const std::string& action,
                             const base::DictionaryValue& extras) const;

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;

  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      reporting_consent_subscription_;
  ArcBridgeService* const arc_bridge_service_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // WeakPtrFactory to use for callback for getting the bluetooth adapter.
  base::WeakPtrFactory<ArcSettingsServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsServiceImpl);
};

ArcSettingsServiceImpl::ArcSettingsServiceImpl(
    ArcBridgeService* arc_bridge_service)
    : arc_bridge_service_(arc_bridge_service), weak_factory_(this) {
  StartObservingSettingsChanges();
  SyncAllPrefs();
}

ArcSettingsServiceImpl::~ArcSettingsServiceImpl() {
  StopObservingSettingsChanges();

  if (bluetooth_adapter_) {
    bluetooth_adapter_->RemoveObserver(this);
  }
}

void ArcSettingsServiceImpl::StartObservingSettingsChanges() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  registrar_.Init(profile->GetPrefs());

  AddPrefToObserve(prefs::kWebKitDefaultFixedFontSize);
  AddPrefToObserve(prefs::kWebKitDefaultFontSize);
  AddPrefToObserve(prefs::kWebKitMinimumFontSize);
  AddPrefToObserve(prefs::kAccessibilitySpokenFeedbackEnabled);
  AddPrefToObserve(prefs::kUse24HourClock);
  AddPrefToObserve(proxy_config::prefs::kProxy);

  reporting_consent_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      chromeos::kStatsReportingPref,
      base::Bind(&ArcSettingsServiceImpl::SyncReportingConsent,
                 base::Unretained(this)));

  TimezoneSettings::GetInstance()->AddObserver(this);

  if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&ArcSettingsServiceImpl::OnBluetoothAdapterInitialized,
                   weak_factory_.GetWeakPtr()));
  }
}

void ArcSettingsServiceImpl::OnBluetoothAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter);
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);

  AdapterPoweredChanged(adapter.get(), adapter->IsPowered());
}

void ArcSettingsServiceImpl::SyncAllPrefs() const {
  SyncFontSize();
  SyncLocale();
  SyncProxySettings();
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

void ArcSettingsServiceImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  base::DictionaryValue extras;
  extras.SetBoolean("enable", powered);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_BLUETOOTH_STATE",
                        extras);
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
  } else if (pref_name == proxy_config::prefs::kProxy) {
    SyncProxySettings();
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

void ArcSettingsServiceImpl::SyncProxySettings() const {
  const PrefService::Preference* const pref =
      registrar_.prefs()->FindPreference(proxy_config::prefs::kProxy);
  const base::DictionaryValue* proxy_config_value;
  bool value_exists = pref->GetValue()->GetAsDictionary(&proxy_config_value);
  DCHECK(value_exists);

  ProxyConfigDictionary proxy_config_dict(proxy_config_value);

  ProxyPrefs::ProxyMode mode;
  if (!proxy_config_dict.GetMode(&mode))
    mode = ProxyPrefs::MODE_DIRECT;

  base::DictionaryValue extras;
  extras.SetString("mode", ProxyPrefs::ProxyModeToString(mode));

  switch (mode) {
    case ProxyPrefs::MODE_DIRECT:
      break;
    case ProxyPrefs::MODE_SYSTEM:
      LOG(WARNING) << "The system mode is not translated.";
      return;
    case ProxyPrefs::MODE_AUTO_DETECT:
      extras.SetString("pacUrl", "http://wpad/wpad.dat");
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url;
      if (!proxy_config_dict.GetPacUrl(&pac_url)) {
        LOG(ERROR) << "No pac URL for pac_script proxy mode.";
        return;
      }
      extras.SetString("pacUrl", pac_url);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string host;
      int port = 0;
      if (!GetHttpProxyServer(proxy_config_dict, &host, &port)) {
        LOG(ERROR) << "No Http proxy server is sent.";
        return;
      }
      extras.SetString("host", host);
      extras.SetInteger("port", port);

      std::string bypass_list;
      if (proxy_config_dict.GetBypassList(&bypass_list) &&
          !bypass_list.empty()) {
        extras.SetString("bypassList", bypass_list);
      }
      break;
    }
    default:
      LOG(ERROR) << "Incorrect proxy mode.";
      return;
  }

  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_PROXY", extras);
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
