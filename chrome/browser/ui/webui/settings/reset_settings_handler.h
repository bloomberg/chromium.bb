// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace content {
class WebUIDataSource;
}

class AutomaticProfileResetter;
class BrandcodeConfigFetcher;
class ProfileResetter;
class ResettableSettingsSnapshot;

namespace settings {

// Handler for
//  1) 'Reset Profile Settings' dialog
//  2) 'Powerwash' dialog (ChromeOS only)
class ResetSettingsHandler
    : public SettingsPageUIHandler,
      public base::SupportsWeakPtr<ResetSettingsHandler> {
 public:
  explicit ResetSettingsHandler(
      content::WebUIDataSource* html_source, content::WebUI* web_ui);
  ~ResetSettingsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Javascript callback to start clearing data.
  void HandleResetProfileSettings(const base::ListValue* value);

  // Closes the dialog once all requested settings has been reset.
  void OnResetProfileSettingsDone(bool send_feedback);

  // Called when the confirmation box appears.
  void OnShowResetProfileDialog(const base::ListValue* value);

  // Called when the confirmation box disappears.
  void OnHideResetProfileDialog(const base::ListValue* value);

  // Called when BrandcodeConfigFetcher completed fetching settings.
  void OnSettingsFetched();

  // Resets profile settings to default values. |send_settings| is true if user
  // gave his consent to upload broken settings to Google for analysis.
  void ResetProfile(bool send_settings);

  // Sets new values for the feedback area.
  void UpdateFeedbackUI();

#if defined(OS_CHROMEOS)
  // Will be called when powerwash dialog is shown.
  void OnShowPowerwashDialog(const base::ListValue* args);

  // Sets a pref indicating that a factory reset is requested and then requests
  // a restart.
  void HandleFactoryResetRestart(const base::ListValue* args);

  // Whether factory reset can be performed.
  bool allow_powerwash_ = false;
#endif  // defined(OS_CHROMEOS)

  // Destroyed with the Profile, thus it should outlive us. This will be NULL if
  // the underlying profile is off-the-record (e.g. in Guest mode on Chrome OS).
  AutomaticProfileResetter* automatic_profile_resetter_ = nullptr;

  // Records whether or not the Profile Reset confirmation dialog was opened at
  // least once during the lifetime of the settings page.
  bool has_shown_confirmation_dialog_ = false;

  scoped_ptr<ProfileResetter> resetter_;

  scoped_ptr<BrandcodeConfigFetcher> config_fetcher_;

  // Snapshot of settings before profile was reseted.
  scoped_ptr<ResettableSettingsSnapshot> setting_snapshot_;

  // Contains Chrome brand code; empty for organic Chrome.
  std::string brandcode_;

  DISALLOW_COPY_AND_ASSIGN(ResetSettingsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_RESET_SETTINGS_HANDLER_H_
