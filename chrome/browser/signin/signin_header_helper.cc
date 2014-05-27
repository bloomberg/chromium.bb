// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_header_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
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

const char kChromeConnectedHeader[] = "X-Chrome-Connected";
const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";
const char kGaiaIdAttrName[] = "id";
const char kProfileModeAttrName[] = "mode";
const char kEnableAccountConsistencyAttrName[] = "enable_account_consistency";

// Determine the service type that has been passed from GAIA in the header.
signin::GAIAServiceType GetGAIAServiceTypeFromHeader(
    const std::string& header_value) {
  if (header_value == "SIGNOUT")
    return signin::GAIA_SERVICE_TYPE_SIGNOUT;
  else if (header_value == "SIGNOUTOPTIONS_INCOGNITO")
    return signin::GAIA_SERVICE_TYPE_SIGNOUTOPTIONS_INCOGNITO;
  else if (header_value == "ADDSESSION")
    return signin::GAIA_SERVICE_TYPE_ADDSESSION;
  else if (header_value == "REAUTH")
    return signin::GAIA_SERVICE_TYPE_REAUTH;
  else if (header_value == "DEFAULT")
    return signin::GAIA_SERVICE_TYPE_DEFAULT;
  else
    return signin::GAIA_SERVICE_TYPE_NONE;
}

// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeaderUIThread(
    int child_id, int route_id, const std::string& header_value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

  signin::GAIAServiceType service_type =
      GetGAIAServiceTypeFromHeader(header_value);

#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    if (service_type == signin::GAIA_SERVICE_TYPE_SIGNOUTOPTIONS_INCOGNITO) {
      chrome::NewIncognitoWindow(browser);
    } else {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
          service_type);
    }
  }
#else  // defined(OS_ANDROID)
  if (service_type == signin::GAIA_SERVICE_TYPE_SIGNOUTOPTIONS_INCOGNITO) {
    web_contents->OpenURL(content::OpenURLParams(
        GURL(chrome::kChromeUINativeNewTabURL), content::Referrer(),
        OFF_THE_RECORD, content::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  } else {
    // TODO(mlerman): pass service_type to android logic for UMA metrics
    // that will eventually be installed there.
    AccountManagementScreenHelper::OpenAccountManagementScreen(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  }
#endif // OS_ANDROID
}

bool IsDriveOrigin(const GURL& url) {
  if (!url.SchemeIsSecure())
    return false;

  const GURL kGoogleDriveURL("https://drive.google.com");
  const GURL kGoogleDocsURL("https://docs.google.com");
  return url == kGoogleDriveURL || url == kGoogleDocsURL;
}

} // empty namespace

namespace signin {

void AppendMirrorRequestHeaderIfPossible(
    net::URLRequest* request,
    const GURL& redirect_url,
    ProfileIOData* io_data,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (io_data->IsOffTheRecord() ||
      io_data->google_services_username()->GetValue().empty()) {
    return;
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
  bool is_new_profile_management = switches::IsNewProfileManagement();
  bool is_google_url =
      !switches::IsEnableWebBasedSignin() &&
      is_new_profile_management &&
      google_util::IsGoogleDomainUrl(
          url,
          google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS);
  if (!is_google_url && !IsDriveOrigin(origin))
    return;

  std::string account_id(io_data->google_services_account_id()->GetValue());

  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  // TODO(guohui): Needs to check for parent control as well.
  if (io_data->incognito_availibility()->GetValue() ==
          IncognitoModePrefs::DISABLED) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

  // TODO(guohui): needs to make a new flag for enabling account consistency.
  std::string header_value(base::StringPrintf("%s=%s,%s=%s,%s=%s",
      kGaiaIdAttrName, account_id.c_str(),
      kProfileModeAttrName, base::IntToString(profile_mode_mask).c_str(),
      kEnableAccountConsistencyAttrName,
      is_new_profile_management ? "true" : "false"));
  request->SetExtraRequestHeaderByName(
      kChromeConnectedHeader, header_value, false);
}

void ProcessMirrorResponseHeaderIfExists(
    net::URLRequest* request,
    ProfileIOData* io_data,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  std::string header_value;
  if (gaia::IsGaiaSignonRealm(request->url().GetOrigin()) &&
      request->response_headers()->GetNormalizedHeader(
          kChromeManageAccountsHeader, &header_value)) {
    DCHECK(switches::IsNewProfileManagement() &&
           !io_data->IsOffTheRecord());
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(ProcessMirrorHeaderUIThread, child_id, route_id,
                   header_value));
  }
}

} // namespace signin
