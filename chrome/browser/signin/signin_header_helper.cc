// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_header_helper.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#endif  // defined(OS_ANDROID)

namespace {

const char kChromeConnectedHeader[] = "X-Chrome-Connected";
const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";

// Show profile avatar bubble on UI thread. Must be called on the UI thread.
void ShowAvatarBubbleUIThread(int child_id, int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT);
  }
#else  // defined(OS_ANDROID)
  AccountManagementScreenHelper::OpenAccountManagementScreen(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
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
  bool is_google_url =
      !switches::IsEnableWebBasedSignin() &&
      switches::IsNewProfileManagement() &&
      google_util::IsGoogleDomainUrl(
          url,
          google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS);
  if (!is_google_url && !IsDriveOrigin(origin))
    return;

  std::string account_id(io_data->google_services_account_id()->GetValue());
  if (account_id.empty())
    account_id = "1"; // Dummy value if focus ID not available yet.

  request->SetExtraRequestHeaderByName(
      kChromeConnectedHeader, account_id, false);
}

void ProcessMirrorResponseHeaderIfExists(
    net::URLRequest* request,
    ProfileIOData* io_data,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (gaia::IsGaiaSignonRealm(request->url().GetOrigin()) &&
      request->response_headers()->HasHeader(kChromeManageAccountsHeader)) {
    DCHECK(switches::IsNewProfileManagement() &&
           !io_data->IsOffTheRecord());
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(ShowAvatarBubbleUIThread, child_id, route_id));
  }
}

} // namespace signin
