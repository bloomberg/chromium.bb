// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#pragma once

#include "content/browser/webui/web_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Base class for the OOBE/Login WebUI handlers.
class BaseScreenHandler : public WebUIMessageHandler {
 public:
  BaseScreenHandler();
  virtual ~BaseScreenHandler();

  // Gets localized strings to be used on the page.
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) = 0;

  // This method is called when page is ready. It propagates to inherited class
  // via virtual Initialize() method (see below).
  void InitializeBase();

 protected:
  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI |screen|. Optionally it can pass screen initialization
  // data via |data| parameter.
  void ShowScreen(const char* screen, const base::DictionaryValue* data);

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

 private:
  // Keeps whether page is ready.
  bool page_is_ready_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
