// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/options/core_options_handler.h"

namespace chromeos {

// CoreChromeOSOptionsHandler handles ChromeOS settings.
class CoreChromeOSOptionsHandler : public CoreOptionsHandler {
 public:
  CoreChromeOSOptionsHandler();

 protected:
  // ::CoreOptionsHandler overrides
  virtual Value* FetchPref(const std::string& pref_name);
  virtual void ObservePref(const std::string& pref_name);
  virtual void SetPref(const std::string& pref_name,
                       const Value* value,
                       const std::string& metric);
  virtual void StopObservingPref(const std::string& path);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Notifies registered JS callbacks on ChromeOS setting change.
  void NotifySettingsChanged(const std::string* setting_name);

  // Keeps the track of change caused by the handler to make sure
  // it does not signal itself again.
  bool handling_change_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CORE_CHROMEOS_OPTIONS_HANDLER_H_
