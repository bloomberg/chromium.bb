// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_signin_confirmation_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ProfileSigninConfirmationUI::ProfileSigninConfirmationUI(content::WebUI* web_ui)
  : ConstrainedWebDialogUI(web_ui) {
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIProfileSigninConfirmationHost);
  html_source->SetUseJsonJSFormatV2();

  html_source->AddLocalizedString(
      "createProfileButtonText",
      IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE);
  html_source->AddLocalizedString(
      "continueButtonText",
      IDS_ENTERPRISE_SIGNIN_CONTINUE);
  html_source->AddLocalizedString("okButtonText", IDS_OK);
  html_source->AddLocalizedString(
      "cancelButtonText",
      IDS_ENTERPRISE_SIGNIN_CANCEL);
  html_source->AddLocalizedString(
      "learnMoreText",
      IDS_ENTERPRISE_SIGNIN_PROFILE_LINK_LEARN_MORE);
  html_source->AddLocalizedString(
      "dialogTitle",
      IDS_ENTERPRISE_SIGNIN_TITLE);
  html_source->AddLocalizedString(
      "dialogMessage",
      IDS_ENTERPRISE_SIGNIN_EXPLANATION);
  html_source->AddLocalizedString(
      "dialogPrompt",
      IDS_ENTERPRISE_SIGNIN_CREATE_NEW_PROFILE_MESSAGE);

  html_source->SetJsonPath("strings.js");

  html_source->AddResourcePath("profile_signin_confirmation.js",
                               IDR_PROFILE_SIGNIN_CONFIRMATION_JS);
  html_source->AddResourcePath("profile_signin_confirmation.css",
                               IDR_PROFILE_SIGNIN_CONFIRMATION_CSS);
  html_source->SetDefaultResource(IDR_PROFILE_SIGNIN_CONFIRMATION_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

ProfileSigninConfirmationUI::~ProfileSigninConfirmationUI() {
}
