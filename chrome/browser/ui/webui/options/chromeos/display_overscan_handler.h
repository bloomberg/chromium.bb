// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OVERSCAN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OVERSCAN_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/display/display_observer.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {
class OverscanCalibrator;

namespace options {

// Display options overlay page UI handler.
class DisplayOverscanHandler : public ::options::OptionsPageUIHandler,
                               public display::DisplayObserver {
 public:
  DisplayOverscanHandler();
  ~DisplayOverscanHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // display::DisplayObserver implementation.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  // Handlers of JS messages.
  void HandleStart(const base::ListValue* args);
  void HandleCommit(const base::ListValue* unused_args);
  void HandleReset(const base::ListValue* unused_args);
  void HandleCancel(const base::ListValue* unused_args);
  void HandleMove(const base::ListValue* args);
  void HandleResize(const base::ListValue* args);

  std::unique_ptr<OverscanCalibrator> overscan_calibrator_;

  DISALLOW_COPY_AND_ASSIGN(DisplayOverscanHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OVERSCAN_HANDLER_H_
