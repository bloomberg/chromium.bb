// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_

#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {
namespace options {

// Display options overlay page UI handler.
class DisplayOptionsHandler : public ::options::OptionsPageUIHandler,
                              public ash::WindowTreeHostManager::Observer {
 public:
  DisplayOptionsHandler();
  ~DisplayOptionsHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // ash::WindowTreeHostManager::Observer implementation.
  void OnDisplayConfigurationChanging() override;
  void OnDisplayConfigurationChanged() override;

 private:
  // Sends all of the current display information to the web_ui of options page.
  void SendAllDisplayInfo();

  // Enables or disables the display settings UI.
  void UpdateDisplaySettingsEnabled();

  // Handlers of JS messages.
  void HandleDisplayInfo(const base::ListValue* unused_args);
  void HandleMirroring(const base::ListValue* args);
  void HandleSetPrimary(const base::ListValue* args);
  void HandleSetDisplayLayout(const base::ListValue* args);
  void HandleSetDisplayMode(const base::ListValue* args);
  void HandleSetRotation(const base::ListValue* args);
  void HandleSetColorProfile(const base::ListValue* args);
  void HandleSetUnifiedDesktopEnabled(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(DisplayOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
