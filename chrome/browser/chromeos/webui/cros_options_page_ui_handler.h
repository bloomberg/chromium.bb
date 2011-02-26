// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_CROS_OPTIONS_PAGE_UI_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_CROS_OPTIONS_PAGE_UI_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace chromeos {

// The base class handler of Javascript messages of options pages.
class CrosOptionsPageUIHandler : public OptionsPageUIHandler {
 public:
  explicit CrosOptionsPageUIHandler(CrosSettingsProvider* provider);
  virtual ~CrosOptionsPageUIHandler();

 protected:
  scoped_ptr<CrosSettingsProvider> settings_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosOptionsPageUIHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_CROS_OPTIONS_PAGE_UI_HANDLER_H_
