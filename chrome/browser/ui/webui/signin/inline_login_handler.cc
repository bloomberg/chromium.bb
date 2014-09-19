// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

InlineLoginHandler::InlineLoginHandler() {}

InlineLoginHandler::~InlineLoginHandler() {}

void InlineLoginHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("initialize",
      base::Bind(&InlineLoginHandler::HandleInitializeMessage,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback("completeLogin",
      base::Bind(&InlineLoginHandler::HandleCompleteLoginMessage,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "switchToFullTab",
      base::Bind(&InlineLoginHandler::HandleSwitchToFullTabMessage,
                 base::Unretained(this)));
}

void InlineLoginHandler::HandleInitializeMessage(const base::ListValue* args) {
  base::DictionaryValue params;

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  params.SetString("hl", app_locale);
  GaiaUrls* gaiaUrls = GaiaUrls::GetInstance();
  params.SetString("gaiaUrl", gaiaUrls->gaia_url().spec());
  params.SetInteger("authMode", InlineLoginHandler::kDesktopAuthMode);

  const GURL& current_url = web_ui()->GetWebContents()->GetURL();
  signin::Source source = signin::GetSourceForPromoURL(current_url);
  if (source == signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT ||
      source == signin::SOURCE_AVATAR_BUBBLE_SIGN_IN ||
      source == signin::SOURCE_REAUTH) {
    // Drop the leading slash in the path.
    params.SetString(
        "gaiaPath",
        GaiaUrls::GetInstance()->embedded_signin_url().path().substr(1));
  }

  params.SetString(
      "continueUrl",
      signin::GetLandingURL(signin::kSignInPromoQueryKeySource,
                            static_cast<int>(source)).spec());

  std::string default_email;
  if (source != signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT &&
      source != signin::SOURCE_REAUTH) {
    default_email = Profile::FromWebUI(web_ui())->GetPrefs()->GetString(
        prefs::kGoogleServicesLastUsername);
  } else {
    if (!net::GetValueForKeyInQuery(current_url, "email", &default_email))
      default_email.clear();
  }
  if (!default_email.empty())
    params.SetString("email", default_email);

  std::string frame_url;
  net::GetValueForKeyInQuery(current_url, "frameUrl", &frame_url);
  if (!frame_url.empty())
    params.SetString("frameUrl", frame_url);

  std::string is_constrained;
  net::GetValueForKeyInQuery(
      current_url, signin::kSignInPromoQueryKeyConstrained, &is_constrained);
  if (!is_constrained.empty())
    params.SetString(signin::kSignInPromoQueryKeyConstrained, is_constrained);

  // TODO(rogerta): this needs to be passed on to gaia somehow.
  std::string read_only_email;
  net::GetValueForKeyInQuery(current_url, "readOnlyEmail", &read_only_email);
  if (!read_only_email.empty())
    params.SetString("readOnlyEmail", read_only_email);

  SetExtraInitParams(params);

  web_ui()->CallJavascriptFunction("inline.login.loadAuthExtension", params);
}

void InlineLoginHandler::HandleCompleteLoginMessage(
    const base::ListValue* args) {
  CompleteLogin(args);
}

void InlineLoginHandler::HandleSwitchToFullTabMessage(
    const base::ListValue* args) {
  base::string16 url_str;
  CHECK(args->GetString(0, &url_str));

  content::WebContents* web_contents = web_ui()->GetWebContents();
  GURL main_frame_url(web_contents->GetURL());
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, "frameUrl", base::UTF16ToASCII(url_str));

  // Adds extra parameters to the signin URL so that Chrome will close the tab
  // and show the account management view of the avatar menu upon completion.
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyAutoClose, "1");
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyShowAccountManagement, "1");

  chrome::NavigateParams params(
      Profile::FromWebUI(web_ui()),
      net::AppendOrReplaceQueryParameter(
          main_frame_url, signin::kSignInPromoQueryKeyConstrained, "0"),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  chrome::Navigate(&params);

  web_ui()->CallJavascriptFunction("inline.login.closeDialog");
}
