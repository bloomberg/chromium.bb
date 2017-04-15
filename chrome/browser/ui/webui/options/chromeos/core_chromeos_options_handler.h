// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
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
  ~CoreChromeOSOptionsHandler() override;

  // ::CoreOptionsHandler overrides
  void RegisterMessages() override;
  base::Value* FetchPref(const std::string& pref_name) override;
  void InitializeHandler() override;
  void ObservePref(const std::string& pref_name) override;
  void SetPref(const std::string& pref_name,
               const base::Value* value,
               const std::string& metric) override;
  void StopObservingPref(const std::string& path) override;
  base::Value* CreateValueForPref(
      const std::string& pref_name,
      const std::string& controlling_pref_name) override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void OnPreferenceChanged(PrefService* service,
                           const std::string& pref_name) override;

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

  // Currently selected network id.
  std::string network_guid_;

  DISALLOW_COPY_AND_ASSIGN(CoreChromeOSOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
