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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {
namespace options {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public ::options::CoreOptionsHandler,
                                   public content::NotificationObserver {
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
  virtual base::Value* CreateValueForPref(
      const std::string& pref_name,
      const std::string& controlling_pref_name) OVERRIDE;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual void OnPreferenceChanged(PrefService* service,
                                   const std::string& pref_name) OVERRIDE;

  // Called from Javascript to select the network to show proxy settings
  // for. Triggers pref notifications about the updated proxy settings.
  void SelectNetworkCallback(const base::ListValue* args);

  // Notifies registered JS callbacks on ChromeOS setting change.
  void NotifySettingsChanged(const std::string& setting_name);
  void NotifyProxyPrefsChanged();

  // Called on changes to the ownership status. Needed to update the interface
  // in case it has been shown before ownership has been fully established.
  void NotifyOwnershipChanged();

  typedef std::map<std::string, linked_ptr<CrosSettings::ObserverSubscription> >
      SubscriptionMap;
  SubscriptionMap pref_subscription_map_;

  content::NotificationRegistrar notification_registrar_;

  UIProxyConfigService proxy_config_service_;
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
