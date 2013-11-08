// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_header_helper.h"

#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace {

const char kChromeConnectedHeader[] = "X-Chrome-Connected";
const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";
const char kGaiaExperimentHeader[] = "X-Google-Accounts-Experiment";
const char kGaiaExperimentID[] = "SEND_204_FOR_CHROME_MIRROR_ACTIONS";
const char kGaiaAuthExtensionID[] = "mfffpogegjflfpflabcdkioaeobkgjik";

// Show profile avatar bubble on UI thread. Must be called on the UI thread.
void ShowAvatarBubbleUIThread(int child_id, int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if !defined(OS_ANDROID)
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  browser->window()->ShowAvatarBubbleFromAvatarButton();
  // TODO(guohui): need to handle the case when avatar button is not available.
#endif // OS_ANDROID
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

  const GURL& url = redirect_url.is_empty() ? request->url() : redirect_url;
  if (!profiles::IsNewProfileManagementEnabled() ||
      !gaia::IsGaiaSignonRealm(url.GetOrigin()) ||
      io_data->is_incognito() ||
      io_data->google_services_username()->GetValue().empty()) {
    return;
  }

  ExtensionRendererState* renderer_state =
      ExtensionRendererState::GetInstance();
  ExtensionRendererState::WebViewInfo webview_info;
  bool is_guest = renderer_state->GetWebViewInfo(
      child_id, route_id, &webview_info);
  if (is_guest && webview_info.extension_id == kGaiaAuthExtensionID){
    return;
  }

  request->SetExtraRequestHeaderByName(kChromeConnectedHeader, "1", false);
  // Temporary header required by Gaia, since Gaia currently serves header-204
  // as an experiment.
  request->SetExtraRequestHeaderByName(
      kGaiaExperimentHeader, kGaiaExperimentID, false);
}

void ProcessMirrorResponseHeaderIfExists(
    net::URLRequest* request,
    ProfileIOData* io_data,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (gaia::IsGaiaSignonRealm(request->url().GetOrigin()) &&
      request->response_headers()->HasHeader(kChromeManageAccountsHeader)) {
    DCHECK(profiles::IsNewProfileManagementEnabled() &&
           !io_data->is_incognito());
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(ShowAvatarBubbleUIThread, child_id, route_id));
  }
}

} // namespace signin
