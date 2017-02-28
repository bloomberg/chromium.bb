// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler.h"

#include <limits.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

InlineLoginHandler::InlineLoginHandler() : weak_ptr_factory_(this) {}

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
  web_ui()->RegisterMessageCallback("navigationButtonClicked",
      base::Bind(&InlineLoginHandler::HandleNavigationButtonClicked,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("dialogClose",
      base::Bind(&InlineLoginHandler::HandleDialogClose,
                 base::Unretained(this)));
}

void InlineLoginHandler::HandleInitializeMessage(const base::ListValue* args) {
  AllowJavascript();
  content::WebContents* contents = web_ui()->GetWebContents();
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());
  if (partition) {
    const GURL& current_url = web_ui()->GetWebContents()->GetURL();

    // If the kSignInPromoQueryKeyForceKeepData param is missing, or if it is
    // present and its value is zero, this means we don't want to keep the
    // the data.
    std::string value;
    if (!net::GetValueForKeyInQuery(current_url,
                                    signin::kSignInPromoQueryKeyForceKeepData,
                                    &value) ||
        value == "0") {
      partition->ClearData(
          content::StoragePartition::REMOVE_DATA_MASK_ALL,
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
          GURL(),
          content::StoragePartition::OriginMatcherFunction(),
          base::Time(),
          base::Time::Max(),
          base::Bind(&InlineLoginHandler::ContinueHandleInitializeMessage,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      ContinueHandleInitializeMessage();
    }
  }
}

void InlineLoginHandler::RecordSigninUserActionForAccessPoint(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromStartPage"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK:
      content::RecordAction(base::UserMetricsAction("Signin_Signin_FromNTP"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_MENU:
      content::RecordAction(base::UserMetricsAction("Signin_Signin_FromMenu"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSettings"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SUPERVISED_USER:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSupervisedUser"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromExtensionInstallBubble"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromExtensions"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_APPS_PAGE_LINK:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAppsPageLink"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromBookmarkBubble"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromBookmarkManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAvatarBubbleSignin"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromUserManager"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromDevicesPage"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_CLOUD_PRINT:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromCloudPrint"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_CONTENT_AREA:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromContentArea"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromSigninPromo"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RECENT_TABS:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromRecentTabs"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromUnknownAccessPoint"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromPasswordBubble"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromAutofillDropdown"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_NTP_CONTENT_SUGGESTIONS:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromNTPContentSuggestions"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_RESIGNIN_INFOBAR:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromReSigninInfobar"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_TAB_SWITCHER:
      content::RecordAction(
          base::UserMetricsAction("Signin_Signin_FromTabSwitcher"));
      break;
    case signin_metrics::AccessPoint::ACCESS_POINT_MAX:
      NOTREACHED();
      break;
  }
}

void InlineLoginHandler::ContinueHandleInitializeMessage() {
  base::DictionaryValue params;

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  params.SetString("hl", app_locale);
  GaiaUrls* gaiaUrls = GaiaUrls::GetInstance();
  params.SetString("gaiaUrl", gaiaUrls->gaia_url().spec());
  params.SetInteger("authMode", InlineLoginHandler::kDesktopAuthMode);

  const GURL& current_url = web_ui()->GetWebContents()->GetURL();
  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(current_url);
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(current_url);

  if (reason != signin_metrics::Reason::REASON_REAUTHENTICATION &&
      reason != signin_metrics::Reason::REASON_UNLOCK &&
      reason != signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT) {
    signin_metrics::LogSigninAccessPointStarted(access_point);
    RecordSigninUserActionForAccessPoint(access_point);
    content::RecordAction(base::UserMetricsAction("Signin_SigninPage_Loading"));
    params.SetBoolean("isLoginPrimaryAccount", true);
  }

  params.SetString("continueUrl", signin::GetLandingURL(access_point).spec());

  Profile* profile = Profile::FromWebUI(web_ui());
  std::string default_email;
  if (reason == signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT) {
    default_email =
        profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
  } else {
    if (!net::GetValueForKeyInQuery(current_url, "email", &default_email))
      default_email.clear();
  }
  if (!default_email.empty())
    params.SetString("email", default_email);

  std::string is_constrained;
  net::GetValueForKeyInQuery(
      current_url, signin::kSignInPromoQueryKeyConstrained, &is_constrained);
  if (!is_constrained.empty())
    params.SetString(signin::kSignInPromoQueryKeyConstrained, is_constrained);

  // TODO(rogerta): this needs to be passed on to gaia somehow.
  std::string read_only_email;
  net::GetValueForKeyInQuery(current_url, "readOnlyEmail", &read_only_email);
  params.SetBoolean("readOnlyEmail", !read_only_email.empty());

  SetExtraInitParams(params);
  CallJavascriptFunction("inline.login.loadAuthExtension", params);
}

void InlineLoginHandler::HandleCompleteLoginMessage(
    const base::ListValue* args) {
  CompleteLogin(args);
}

void InlineLoginHandler::HandleSwitchToFullTabMessage(
    const base::ListValue* args) {
  std::string url_str;
  CHECK(args->GetString(0, &url_str));

  Profile* profile = Profile::FromWebUI(web_ui());
  GURL main_frame_url(web_ui()->GetWebContents()->GetURL());

  // Adds extra parameters to the signin URL so that Chrome will close the tab
  // and show the account management view of the avatar menu upon completion.
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyAutoClose, "1");
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyShowAccountManagement, "1");
  main_frame_url = net::AppendOrReplaceQueryParameter(
      main_frame_url, signin::kSignInPromoQueryKeyForceKeepData, "1");

  chrome::NavigateParams params(
      profile,
      net::AppendOrReplaceQueryParameter(
          main_frame_url, signin::kSignInPromoQueryKeyConstrained, "0"),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  chrome::Navigate(&params);

  CloseDialogFromJavascript();
}

void InlineLoginHandler::HandleNavigationButtonClicked(
    const base::ListValue* args) {
  Browser* browser = signin::GetDesktopBrowser(web_ui());
  DCHECK(browser);

  browser->signin_view_controller()->delegate()->PerformNavigation();
}

void InlineLoginHandler::HandleDialogClose(const base::ListValue* args) {
  Browser* browser = signin::GetDesktopBrowser(web_ui());
  // If the dialog was opened in the User Manager browser will be null here.
  if (browser)
    browser->CloseModalSigninWindow();

  // Does nothing if user manager is not showing.
  UserManagerProfileDialog::HideDialog();
}

void InlineLoginHandler::CloseDialogFromJavascript() {
  if (IsJavascriptAllowed())
    CallJavascriptFunction("inline.login.closeDialog");
}
