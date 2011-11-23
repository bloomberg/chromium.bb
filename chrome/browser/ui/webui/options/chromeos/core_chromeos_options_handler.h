// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"

class PrefSetObserver;

namespace chromeos {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public CoreOptionsHandler {
 public:
  CoreChromeOSOptionsHandler();
  virtual ~CoreChromeOSOptionsHandler();

 protected:
  // ::CoreOptionsHandler overrides
  virtual void Initialize() OVERRIDE;
  virtual base::Value* FetchPref(const std::string& pref_name) OVERRIDE;
  virtual void ObservePref(const std::string& pref_name) OVERRIDE;
  virtual void SetPref(const std::string& pref_name,
                       const base::Value* value,
                       const std::string& metric) OVERRIDE;
  virtual void StopObservingPref(const std::string& path) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Notifies registered JS callbacks on ChromeOS setting change.
  void NotifySettingsChanged(const std::string* setting_name);
  void NotifyProxyPrefsChanged();

  // Keeps the track of change caused by the handler to make sure
  // it does not signal itself again.
  bool handling_change_;

  scoped_ptr<PrefSetObserver> proxy_prefs_;
  base::WeakPtrFactory<CoreChromeOSOptionsHandler> pointer_factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
