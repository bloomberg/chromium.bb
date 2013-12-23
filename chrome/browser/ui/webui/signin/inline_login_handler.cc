// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"

InlineLoginHandler::InlineLoginHandler() {}

InlineLoginHandler::~InlineLoginHandler() {}

void InlineLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("initialize",
      base::Bind(&InlineLoginHandler::HandleInitializeMessage,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback("completeLogin",
      base::Bind(&InlineLoginHandler::HandleCompleteLoginMessage,
                  base::Unretained(this)));
}

void InlineLoginHandler::HandleInitializeMessage(const base::ListValue* args) {
  base::DictionaryValue params;

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  params.SetString("hl", app_locale);
  GaiaUrls* gaiaUrls = GaiaUrls::GetInstance();
  params.SetString("gaiaUrl", gaiaUrls->gaia_url().spec());
  params.SetInteger("authMode", kDefaultAuthMode);

  SetExtraInitParams(params);

  web_ui()->CallJavascriptFunction("inline.login.loadAuthExtension", params);
}

void InlineLoginHandler::HandleCompleteLoginMessage(
    const base::ListValue* args) {
  CompleteLogin(args);
}
