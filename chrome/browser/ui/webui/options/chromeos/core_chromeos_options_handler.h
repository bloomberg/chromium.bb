// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/ui_proxy_config_service.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"

namespace chromeos {
namespace options {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public ::options::CoreOptionsHandler {
 public:
  CoreChromeOSOptionsHandler();
  virtual ~CoreChromeOSOptionsHandler();

  // ::CoreOptionsHandler overrides
  virtual void RegisterMessages() OVERRIDE;
  virtual base::Value* FetchPref(const std::string& pref_name) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void ObservePref(const std::string& pref_name) OVERRIDE;
  virtual void SetPref(const std::string& pref_name,
                       const base::Value* value,
                       const std::string& metric) OVERRIDE;
  virtual void StopObservingPref(const std::string& path) OVERRIDE;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

 private:
  virtual void OnPreferenceChanged(PrefService* service,
                                   const std::string& pref_name) OVERRIDE;

  // Called from Javascript to select the network to show proxy settings
  // for. Triggers pref notifications about the updated proxy settings.
  void SelectNetworkCallback(const base::ListValue* args);

  // Notifies registered JS callbacks on ChromeOS setting change.
  void NotifySettingsChanged(const std::string& setting_name);
  void NotifyProxyPrefsChanged();

  typedef std::map<std::string, linked_ptr<CrosSettings::ObserverSubscription> >
      SubscriptionMap;
  SubscriptionMap pref_subscription_map_;

  UIProxyConfigService proxy_config_service_;
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
