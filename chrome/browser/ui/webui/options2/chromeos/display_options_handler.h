// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
#pragma once

#include <vector>

#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "ui/aura/display_observer.h"

namespace base {
class ListValue;
}

namespace chromeos {
namespace options2 {

// Display options overlay page UI handler.
class DisplayOptionsHandler : public ::options2::OptionsPageUIHandler,
                              public aura::DisplayObserver {
 public:
  DisplayOptionsHandler();
  virtual ~DisplayOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // aura::DisplayObserver implementation.
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

 private:
  // Updates the display section visibility based on the current display
  // configurations.
  void UpdateDisplaySectionVisibility();

  // Sends the current display information to the web_ui of options page.
  void SendDisplayInfo();

  // Handlers of JS messages.
  void HandleDisplayInfo(const base::ListValue* unused_args);
  void HandleMirroring(const base::ListValue* args);
  void HandleDisplayLayout(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(DisplayOptionsHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
