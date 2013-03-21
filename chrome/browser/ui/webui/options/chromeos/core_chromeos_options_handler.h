// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"

namespace chromeos {
namespace options {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public ::options::CoreOptionsHandler {
 public:
  CoreChromeOSOptionsHandler();
  virtual ~CoreChromeOSOptionsHandler();

  // ::CoreOptionsHandler overrides
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

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual void OnPreferenceChanged(PrefService* service,
                                   const std::string& pref_name) OVERRIDE;

  // Notifies registered JS callbacks on ChromeOS setting change.
  void NotifySettingsChanged(const std::string* setting_name);
  void NotifyProxyPrefsChanged();

  PrefChangeRegistrar proxy_prefs_;
  base::WeakPtrFactory<CoreChromeOSOptionsHandler> pointer_factory_;
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
