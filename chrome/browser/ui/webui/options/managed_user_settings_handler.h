// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_HANDLER_H_

#include "base/prefs/public/pref_change_registrar.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

class ManagedUserSettingsHandler : public OptionsPageUIHandler {
 public:
  ManagedUserSettingsHandler();
  virtual ~ManagedUserSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Save user metrics. Called from WebUI.
  void SaveMetrics(const base::ListValue* args);

  // Called when the local passphrase changes.
  void OnLocalPassphraseChanged();

  // For tracking how long the user spends on this page.
  base::TimeTicks start_time_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_HANDLER_H_
