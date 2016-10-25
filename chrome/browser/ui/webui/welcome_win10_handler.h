// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_WIN10_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_WIN10_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Handles actions on the Windows 10 specific Welcome page.
class WelcomeWin10Handler : public content::WebUIMessageHandler {
 public:
  WelcomeWin10Handler();
  ~WelcomeWin10Handler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleSetDefaultBrowser(const base::ListValue* args);
  void HandleContinue(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(WelcomeWin10Handler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_WIN10_HANDLER_H_
