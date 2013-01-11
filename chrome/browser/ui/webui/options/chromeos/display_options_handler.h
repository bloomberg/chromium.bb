// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_

#include <vector>

#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/gfx/display_observer.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {
class OverscanCalibrator;

namespace options {

// Display options overlay page UI handler.
class DisplayOptionsHandler : public ::options::OptionsPageUIHandler,
                              public gfx::DisplayObserver {
 public:
  DisplayOptionsHandler();
  virtual ~DisplayOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // gfx::DisplayObserver implementation.
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

 private:
  // Updates the display section visibility based on the current display
  // configurations. Specify the number of display.
  void UpdateDisplaySectionVisibility(size_t num_displays);

  // Sends all of the current display information to the web_ui of options page.
  void SendAllDisplayInfo();

  // Sends the specified display information to the web_ui of options page.
  void SendDisplayInfo(const std::vector<const gfx::Display*> displays);

  // Called when the fade-out animation for mirroring status change is finished.
  void OnFadeOutForMirroringFinished(bool is_mirroring);

  // Called when the fade-out animation for secondary display layout change is
  // finished.  |layout| specifies the four positions of the secondary display
  // (left/right/top/bottom), and |offset| is the offset length from the
  // left/top edge of the primary display.
  void OnFadeOutForDisplayLayoutFinished(int layout, int offset);

  // Handlers of JS messages.
  void HandleDisplayInfo(const base::ListValue* unused_args);
  void HandleMirroring(const base::ListValue* args);
  void HandleSetPrimary(const base::ListValue* args);
  void HandleDisplayLayout(const base::ListValue* args);
  void HandleStartOverscanCalibration(const base::ListValue* args);
  void HandleFinishOverscanCalibration(const base::ListValue* args);
  void HandleClearOverscanCalibration(const base::ListValue* args);
  void HandleUpdateOverscanCalibration(const base::ListValue* args);

  scoped_ptr<OverscanCalibrator> overscan_calibrator_;

  DISALLOW_COPY_AND_ASSIGN(DisplayOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OPTIONS_HANDLER_H_
