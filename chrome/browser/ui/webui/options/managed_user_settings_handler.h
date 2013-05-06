// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_HANDLER_H_

#include "base/prefs/pref_change_registrar.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

class ManagedUserSettingsHandler : public OptionsPageUIHandler {
 public:
  ManagedUserSettingsHandler();
  virtual ~ManagedUserSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 private:
  // WebUI message handlers:
  // Save user metrics. Takes no arguments.
  void SaveMetrics(const base::ListValue* args);

  // Sets a managed user setting. The first argument is the name of the
  // setting, the second argument is the Value.
  void SetSetting(const base::ListValue* args);

  // Records metric that the settings page was opened. Takes no arguments.
  void HandlePageOpened(const base::ListValue* args);

  // Decides whether a given pattern is valid, or if it should be
  // rejected. Called while the user is editing an exception pattern.
  void CheckManualExceptionValidity(const base::ListValue* args);

  // Sets the manual behavior for |pattern|. If pattern looks like a host (no
  // schema) then update the manual host list, otherwise update the manual
  // URL list.
  void UpdateManualBehavior(std::string pattern,
                            ManagedUserService::ManualBehavior behavior);

  // Removes the given row from the table. The first entry in |args| is
  // the pattern to remove.
  void RemoveManualException(const base::ListValue* args);

  // Changes the value of an exception. Called after the user is done editing an
  // exception.
  void SetManualException(const base::ListValue* args);

  // Updates the current view by reading the entries from the managed mode
  // service and updating the WebUI model.
  void UpdateViewFromModel();

  // Stores if the user has already seen the managed user settings dialog. Is
  // set to true when the page is opened, and the page had been opened before.
  bool has_seen_settings_dialog_;

  // For tracking how long the user spends on this page.
  base::TimeTicks start_time_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_HANDLER_H_
