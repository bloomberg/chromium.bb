// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_internals_ui.h"

#include "base/hash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/signin_internals_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

content::WebUIDataSource* CreateSignInInternalsHTMLSource() {
  content::WebUIDataSource* source =
      ChromeWebUIDataSource::Create(chrome::kChromeUISignInInternalsHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("signin_internals.js",
                            IDR_SIGNIN_INTERNALS_INDEX_JS);
  source->SetDefaultResource(IDR_SIGNIN_INTERNALS_INDEX_HTML);
  return source;
}

} //  namespace

SignInInternalsUI::SignInInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddWebUIDataSource(
      profile, CreateSignInInternalsHTMLSource());
  if (profile) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile);
    if (about_signin_internals)
      about_signin_internals->AddSigninObserver(this);
  }
}

SignInInternalsUI::~SignInInternalsUI() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile);
    if (about_signin_internals) {
      about_signin_internals->RemoveSigninObserver(this);
    }
  }
}

bool SignInInternalsUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                                   const std::string& name,
                                                   const ListValue& content) {
  if (name == "getSigninInfo") {
    Profile* profile = Profile::FromWebUI(web_ui());
    if (!profile)
      return false;

    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile);
    // TODO(vishwath): The UI would look better if we passed in a dict with some
    // reasonable defaults, so the about:signin-internals page doesn't look
    // empty in incognito mode. Alternatively, we could force about:signin to
    // open in non-incognito mode always (like about:settings for ex.).
    if (about_signin_internals) {
      const std::string& reply_handler =
          "chrome.signin.getSigninInfo.handleReply";
      web_ui()->CallJavascriptFunction(
          reply_handler, *about_signin_internals->GetSigninStatus());

      return true;
    }
  }
  return false;
}

void SignInInternalsUI::OnSigninStateChanged(scoped_ptr<DictionaryValue> info) {
  const std::string& event_handler = "chrome.signin.onSigninInfoChanged.fire";
  web_ui()->CallJavascriptFunction(event_handler, *info);
}
