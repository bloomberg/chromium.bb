// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_header_helper.h"

#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/profile_management_switches.h"
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
const char kGaiaAuthExtensionID[] = "mfffpogegjflfpflabcdkioaeobkgjik";

// Show profile avatar bubble on UI thread. Must be called on the UI thread.
void ShowAvatarBubbleUIThread(int child_id, int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    browser->window()->ShowAvatarBubbleFromAvatarButton();
  // TODO(guohui): need to handle the case when avatar button is not available.
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

   if (io_data->is_incognito() ||
       io_data->google_services_username()->GetValue().empty()) {
     return;
   }

  // Only set the header for Gaia (in the mirror world) and Drive. Gaia needs
  // the header to redirect certain user actions to Chrome native UI. Drive
  // needs the header to tell if the current user is connected. The drive path
  // is a temporary workaround until the more generic chrome.principals API is
  // available.
  const GURL& url = redirect_url.is_empty() ? request->url() : redirect_url;
  GURL origin(url.GetOrigin());
  bool is_gaia_origin = !switches::IsEnableWebBasedSignin() &&
      switches::IsNewProfileManagement() &&
      gaia::IsGaiaSignonRealm(origin);
  if (!is_gaia_origin && !IsDriveOrigin(origin))
    return;

  ExtensionRendererState* renderer_state =
      ExtensionRendererState::GetInstance();
  ExtensionRendererState::WebViewInfo webview_info;
  bool is_guest = renderer_state->GetWebViewInfo(
      child_id, route_id, &webview_info);
  if (is_guest && webview_info.extension_id == kGaiaAuthExtensionID){
    return;
  }

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
           !io_data->is_incognito());
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(ShowAvatarBubbleUIThread, child_id, route_id));
  }
}

} // namespace signin
