// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/url_request/url_request.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/signin/account_management_screen_helper.h"
#else
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif  // defined(OS_ANDROID)

namespace signin {

namespace {

// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeaderUIThread(int child_id,
                                 int route_id,
                                 ManageAccountsParams manage_accounts_params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GAIAServiceType service_type = manage_accounts_params.service_type;
  DCHECK_NE(GAIA_SERVICE_TYPE_NONE, service_type);

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(child_id, route_id);
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnReceivedManageAccountsResponse(
      manage_accounts_params.service_type);
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    BrowserWindow::AvatarBubbleMode bubble_mode;
    switch (service_type) {
      case GAIA_SERVICE_TYPE_INCOGNITO:
        chrome::NewIncognitoWindow(browser);
        return;
      case GAIA_SERVICE_TYPE_ADDSESSION:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT;
        break;
      case GAIA_SERVICE_TYPE_REAUTH:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_REAUTH;
        break;
      default:
        bubble_mode = BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT;
    }
    signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
        account_reconcilor->GetState());
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        bubble_mode, manage_accounts_params,
        signin_metrics::AccessPoint::ACCESS_POINT_CONTENT_AREA, false);
  }
#else   // defined(OS_ANDROID)
  if (service_type == signin::GAIA_SERVICE_TYPE_INCOGNITO) {
    GURL url(manage_accounts_params.continue_url.empty()
                 ? chrome::kChromeUINativeNewTabURL
                 : manage_accounts_params.continue_url);
    web_contents->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::OFF_THE_RECORD,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false));
  } else {
    signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
        account_reconcilor->GetState());
    AccountManagementScreenHelper::OpenAccountManagementScreen(profile,
                                                               service_type);
  }
#endif  // !defined(OS_ANDROID)
}

// Returns the parameters contained in the X-Chrome-Manage-Accounts response
// header.
// If the request does not have a response header or if the header contains
// garbage, then |service_type| is set to |GAIA_SERVICE_TYPE_NONE|.
// Must be called on IO thread.
ManageAccountsParams BuildManageAccountsParamsHelper(net::URLRequest* request,
                                                     ProfileIOData* io_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!(info && info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME)) {
    ManageAccountsParams empty_params;
    empty_params.service_type = GAIA_SERVICE_TYPE_NONE;
    return empty_params;
  }

  return BuildManageAccountsParamsIfExists(request, io_data->IsOffTheRecord());
}

}  // namespace

void FixMirrorRequestHeaderHelper(net::URLRequest* request,
                                  const GURL& redirect_url,
                                  ProfileIOData* io_data,
                                  int child_id,
                                  int route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (io_data->IsOffTheRecord())
    return;

#if !defined(OS_ANDROID)
  extensions::WebViewRendererState::WebViewInfo webview_info;
  bool is_guest = extensions::WebViewRendererState::GetInstance()->GetInfo(
      child_id, route_id, &webview_info);
  // Do not set the x-chrome-connected header on requests from a native signin
  // webview, as identified by an empty extension id which means the webview is
  // embedded in a webui page, otherwise user may end up with a blank page as
  // gaia uses the header to decide whether it returns 204 for certain end
  // points.
  if (is_guest && webview_info.owner_host.empty())
    return;
#endif  // !defined(OS_ANDROID)

  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  if (io_data->incognito_availibility()->GetValue() ==
          IncognitoModePrefs::DISABLED ||
      IncognitoModePrefs::ArePlatformParentalControlsEnabled()) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

  // If new url is eligible to have the header, add it, otherwise remove it.
  AppendOrRemoveMirrorRequestHeaderIfPossible(
      request, redirect_url, io_data->google_services_account_id()->GetValue(),
      io_data->GetCookieSettings(), profile_mode_mask);
}

void ProcessMirrorResponseHeaderIfExists(net::URLRequest* request,
                                         ProfileIOData* io_data,
                                         int child_id,
                                         int route_id) {
  ManageAccountsParams params =
      BuildManageAccountsParamsHelper(request, io_data);
  if (params.service_type == GAIA_SERVICE_TYPE_NONE)
    return;

  params.child_id = child_id;
  params.route_id = route_id;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(ProcessMirrorHeaderUIThread, child_id, route_id, params));
}

}  // namespace signin
