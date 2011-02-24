// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_SYSTEM_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_SYSTEM_OPTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/webui/cros_options_page_ui_handler.h"

class DictionaryValue;

// ChromeOS system options page UI handler.
class SystemOptionsHandler : public chromeos::CrosOptionsPageUIHandler {
 public:
  SystemOptionsHandler();
  virtual ~SystemOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  virtual void RegisterMessages();

  // Called when the accessibility checkbox value is changed.
  // |args| will contain the checkbox checked state as a string
  // ("true" or "false").
  void AccessibilityChangeCallback(const ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_SYSTEM_OPTIONS_HANDLER_H_
