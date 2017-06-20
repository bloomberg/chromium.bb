// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_dice_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/signin_dice_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

SigninDiceInternalsUI::SigninDiceInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DCHECK(!profile->IsOffTheRecord());

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUISigninDiceInternalsHost);
  source->AddResourcePath("signin_dice_internals.js",
                          IDR_SIGNIN_DICE_INTERNALS_JS);
  source->SetDefaultResource(IDR_SIGNIN_DICE_INTERNALS_HTML);
  content::WebUIDataSource::Add(profile, source);

  web_ui->AddMessageHandler(
      base::MakeUnique<SigninDiceInternalsHandler>(profile));
}

SigninDiceInternalsUI::~SigninDiceInternalsUI() {}
