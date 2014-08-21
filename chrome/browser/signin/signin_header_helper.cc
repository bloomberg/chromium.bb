// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_header_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#endif  // defined(OS_ANDROID)

namespace {

// Dictionary of fields in a mirror response header.
typedef std::map<std::string, std::string> MirrorResponseHeaderDictionary;

const char kChromeConnectedHeader[] = "X-Chrome-Connected";
const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";
const char kGaiaIdAttrName[] = "id";
const char kProfileModeAttrName[] = "mode";
const char kEnableAccountConsistencyAttrName[] = "enable_account_consistency";

const char kServiceTypeAttrName[] = "action";
const char kEmailAttrName[] = "email";
const char kIsSamlAttrName[] = "is_saml";
const char kContinueUrlAttrName[] = "continue_url";
const char kIsSameTabAttrName[] = "is_same_tab";

// Determines the service type that has been passed from GAIA in the header.
signin::GAIAServiceType GetGAIAServiceTypeFromHeader(
    const std::string& header_value) {
  if (header_value == "SIGNOUT")
    return signin::GAIA_SERVICE_TYPE_SIGNOUT;
  else if (header_value == "INCOGNITO")
    return signin::GAIA_SERVICE_TYPE_INCOGNITO;
  else if (header_value == "ADDSESSION")
    return signin::GAIA_SERVICE_TYPE_ADDSESSION;
  else if (header_value == "REAUTH")
    return signin::GAIA_SERVICE_TYPE_REAUTH;
  else if (header_value == "SIGNUP")
    return signin::GAIA_SERVICE_TYPE_SIGNUP;
  else if (header_value == "DEFAULT")
    return signin::GAIA_SERVICE_TYPE_DEFAULT;
  else
    return signin::GAIA_SERVICE_TYPE_NONE;
}

// Parses the mirror response header. Its expected format is
// "key1=value1,key2=value2,...".
MirrorResponseHeaderDictionary ParseMirrorResponseHeader(
    const std::string& header_value) {
  std::vector<std::string> fields;
  if (!Tokenize(header_value, std::string(","), &fields))
    return MirrorResponseHeaderDictionary();

  MirrorResponseHeaderDictionary dictionary;
  for (std::vector<std::string>::iterator i = fields.begin();
       i != fields.end(); ++i) {
    std::string field(*i);
    std::vector<std::string> tokens;
    if (Tokenize(field, "=", &tokens) != 2) {
      DLOG(WARNING) << "Unexpected GAIA header field '" << field << "'.";
      continue;
    }
    dictionary[tokens[0]] = tokens[1];
  }
  return dictionary;
}

// Returns the parameters contained in the X-Chrome-Manage-Accounts response
// header.
signin::ManageAccountsParams BuildManageAccountsParams(
    const std::string& header_value) {
  signin::ManageAccountsParams params;
  MirrorResponseHeaderDictionary header_dictionary =
      ParseMirrorResponseHeader(header_value);
  MirrorResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    if (key_name == kServiceTypeAttrName) {
      params.service_type =
          GetGAIAServiceTypeFromHeader(header_dictionary[kServiceTypeAttrName]);
    } else if (key_name == kEmailAttrName) {
      params.email = header_dictionary[kEmailAttrName];
    } else if (key_name == kIsSamlAttrName) {
      params.is_saml = header_dictionary[kIsSamlAttrName] == "true";
    } else if (key_name == kContinueUrlAttrName) {
      params.continue_url = header_dictionary[kContinueUrlAttrName];
    } else if (key_name == kIsSameTabAttrName) {
      params.is_same_tab = header_dictionary[kIsSameTabAttrName] == "true";
    } else {
      DLOG(WARNING) << "Unexpected GAIA header attribute '" << key_name << "'.";
    }
  }
  return params;
}

#if !defined(OS_IOS)
// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeaderUIThread(
    int child_id, int route_id,
    signin::ManageAccountsParams manage_accounts_params) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  signin::GAIAServiceType service_type = manage_accounts_params.service_type;
  DCHECK_NE(signin::GAIA_SERVICE_TYPE_NONE, service_type);

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    BrowserWindow::AvatarBubbleMode bubble_mode;
    switch (service_type) {
      case signin::GAIA_SERVICE_TYPE_INCOGNITO:
        chrome::NewIncognitoWindow(browser);
        return;
      case signin::GAIA_SERVICE_TYPE_ADDSESSION:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN;
        break;
      case signin::GAIA_SERVICE_TYPE_REAUTH:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH;
        break;
      default:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT;
    }
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        bubble_mode, manage_accounts_params);
  }
#else  // defined(OS_ANDROID)
  if (service_type == signin::GAIA_SERVICE_TYPE_INCOGNITO) {
    web_contents->OpenURL(content::OpenURLParams(
        GURL(chrome::kChromeUINativeNewTabURL), content::Referrer(),
        OFF_THE_RECORD, content::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  } else {
    AccountManagementScreenHelper::OpenAccountManagementScreen(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()),
        service_type);
  }
#endif // OS_ANDROID
}
#endif // !defined(OS_IOS)

bool IsDriveOrigin(const GURL& url) {
  if (!url.SchemeIsSecure())
    return false;

  const GURL kGoogleDriveURL("https://drive.google.com");
  const GURL kGoogleDocsURL("https://docs.google.com");
  return url == kGoogleDriveURL || url == kGoogleDocsURL;
}

} // empty namespace

namespace signin {

ManageAccountsParams::ManageAccountsParams() :
    service_type(GAIA_SERVICE_TYPE_NONE),
    email(""),
    is_saml(false),
    continue_url(""),
    is_same_tab(false),
    child_id(0),
    route_id(0) {}

bool AppendMirrorRequestHeaderIfPossible(
    net::URLRequest* request,
    const GURL& redirect_url,
    ProfileIOData* io_data,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (io_data->IsOffTheRecord() ||
      io_data->google_services_username()->GetValue().empty()) {
    return false;
  }

  // Only set the header for Drive always, and other Google properties if
  // new-profile-management is enabled.
  // Vasquette, which is integrated with most Google properties, needs the
  // header to redirect certain user actions to Chrome native UI. Drive needs
  // the header to tell if the current user is connected. The drive path is a
  // temporary workaround until the more generic chrome.principals API is
  // available.
  const GURL& url = redirect_url.is_empty() ? request->url() : redirect_url;
  GURL origin(url.GetOrigin());
  bool is_enable_account_consistency = switches::IsEnableAccountConsistency();
  bool is_google_url =
      !switches::IsEnableWebBasedSignin() &&
      is_enable_account_consistency &&
      (google_util::IsGoogleDomainUrl(
           url,
           google_util::ALLOW_SUBDOMAIN,
           google_util::DISALLOW_NON_STANDARD_PORTS) ||
       google_util::IsYoutubeDomainUrl(
           url,
           google_util::ALLOW_SUBDOMAIN,
           google_util::DISALLOW_NON_STANDARD_PORTS));
  if (!is_google_url && !IsDriveOrigin(origin))
    return false;

  std::string account_id(io_data->google_services_account_id()->GetValue());

  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  if (io_data->incognito_availibility()->GetValue() ==
          IncognitoModePrefs::DISABLED ||
      IncognitoModePrefs::ArePlatformParentalControlsEnabledCached()) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

  // TODO(guohui): needs to make a new flag for enabling account consistency.
  std::string header_value(base::StringPrintf("%s=%s,%s=%s,%s=%s",
      kGaiaIdAttrName, account_id.c_str(),
      kProfileModeAttrName, base::IntToString(profile_mode_mask).c_str(),
      kEnableAccountConsistencyAttrName,
      is_enable_account_consistency ? "true" : "false"));
  request->SetExtraRequestHeaderByName(
      kChromeConnectedHeader, header_value, false);
  return true;
}

void ProcessMirrorResponseHeaderIfExists(
    net::URLRequest* request,
    ProfileIOData* io_data,
    int child_id,
    int route_id) {
#if defined(OS_IOS)
  NOTREACHED();
#else
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!gaia::IsGaiaSignonRealm(request->url().GetOrigin()))
    return;

  std::string header_value;
  if (!request->response_headers()->GetNormalizedHeader(
          kChromeManageAccountsHeader, &header_value)) {
    return;
  }

  DCHECK(switches::IsEnableAccountConsistency() && !io_data->IsOffTheRecord());
  ManageAccountsParams params(BuildManageAccountsParams(header_value));
  if (params.service_type == GAIA_SERVICE_TYPE_NONE)
    return;

  params.child_id = child_id;
  params.route_id = route_id;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(ProcessMirrorHeaderUIThread, child_id, route_id, params));
#endif  // defined(OS_IOS)
}

} // namespace signin
