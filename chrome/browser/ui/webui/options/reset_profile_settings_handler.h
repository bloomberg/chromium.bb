// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_RESET_PROFILE_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_RESET_PROFILE_SETTINGS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

class ProfileResetter;

namespace options {

// Reset Profile Settings handler page UI handler.
class ResetProfileSettingsHandler
    : public OptionsPageUIHandler,
      public base::SupportsWeakPtr<ResetProfileSettingsHandler> {
 public:
  ResetProfileSettingsHandler();
  virtual ~ResetProfileSettingsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Javascript callback to start clearing data.
  void HandleResetProfileSettings(const base::ListValue* value);

  // Closes the dialog once all requested settings has been reset.
  void OnResetProfileSettingsDone();

  scoped_ptr<ProfileResetter> resetter_;

  DISALLOW_COPY_AND_ASSIGN(ResetProfileSettingsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_RESET_PROFILE_SETTINGS_HANDLER_H_
