// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome_win10_handler.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"

WelcomeWin10Handler::WelcomeWin10Handler() = default;

WelcomeWin10Handler::~WelcomeWin10Handler() = default;

// Override from WebUIMessageHandler.
void WelcomeWin10Handler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "handleSetDefaultBrowser",
      base::Bind(&WelcomeWin10Handler::HandleSetDefaultBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "handleContinue",
      base::Bind(&WelcomeWin10Handler::HandleContinue, base::Unretained(this)));
}

void WelcomeWin10Handler::HandleSetDefaultBrowser(const base::ListValue* args) {
  // The worker owns itself.
  (new shell_integration::DefaultBrowserWorker(
       shell_integration::DefaultWebClientWorkerCallback()))
      ->StartSetAsDefault();
}

void WelcomeWin10Handler::HandleContinue(const base::ListValue* args) {
  web_ui()->GetWebContents()->GetController().LoadURL(
      GURL(chrome::kChromeUINewTabURL), content::Referrer(),
      ui::PageTransition::PAGE_TRANSITION_LINK, std::string());
}
