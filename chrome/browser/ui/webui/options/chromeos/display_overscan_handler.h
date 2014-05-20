// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OVERSCAN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OVERSCAN_HANDLER_H_

#include "base/memory/scoped_ptr.h"
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
class DisplayOverscanHandler : public ::options::OptionsPageUIHandler,
                               public gfx::DisplayObserver {
 public:
  DisplayOverscanHandler();
  virtual ~DisplayOverscanHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // gfx::DisplayObserver implementation.
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

 private:
  // Handlers of JS messages.
  void HandleStart(const base::ListValue* args);
  void HandleCommit(const base::ListValue* unused_args);
  void HandleReset(const base::ListValue* unused_args);
  void HandleCancel(const base::ListValue* unused_args);
  void HandleMove(const base::ListValue* args);
  void HandleResize(const base::ListValue* args);

  scoped_ptr<OverscanCalibrator> overscan_calibrator_;

  DISALLOW_COPY_AND_ASSIGN(DisplayOverscanHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_DISPLAY_OVERSCAN_HANDLER_H_
