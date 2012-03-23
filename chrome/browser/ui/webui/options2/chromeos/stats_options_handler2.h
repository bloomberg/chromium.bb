// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_STATS_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_STATS_OPTIONS_HANDLER2_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"

namespace chromeos {
namespace options2 {

// ChromeOS handler for "Stats/crash reporting to Google" option of the Advanced
// settings page. This handler does only ChromeOS-specific actions while default
// code is in Chrome's AdvancedOptionsHandler
// (chrome/browser/webui/advanced_options_handler.cc).
class StatsOptionsHandler : public ::options2::OptionsPageUIHandler {
 public:
  StatsOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  void HandleMetricsReportingCheckbox(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(StatsOptionsHandler);
};

}  // namespace options2
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_CHROMEOS_STATS_OPTIONS_HANDLER2_H_
