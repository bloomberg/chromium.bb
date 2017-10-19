// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_helper.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/process_dice_header_observer_impl.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/http/http_response_headers.h"
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

const char kChromeManageAccountsHeader[] = "X-Chrome-Manage-Accounts";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kGoogleSignoutResponseHeader[] = "Google-Accounts-SignOut";
#endif

// Key for DiceURLRequestUserData.
const void* const kDiceURLRequestUserDataKey = &kDiceURLRequestUserDataKey;

// TODO(droger): Remove this delay when the Dice implementation is finished on
// the server side.
int g_dice_account_reconcilor_blocked_delay_ms = 1000;

// Refcounted wrapper to allow creating and deleting a AccountReconcilor::Lock
// from the IO thread.
class AccountReconcilorLockWrapper
    : public base::RefCountedThreadSafe<AccountReconcilorLockWrapper> {
 public:
  AccountReconcilorLockWrapper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Do nothing on the IO thread. The real work is done in CreateLockOnUI().
  }

  // Creates the account reconcilor lock on the UI thread. The lock will be
  // deleted on the UI thread when this wrapper is deleted.
  void CreateLockOnUI(const content::ResourceRequestInfo::WebContentsGetter&
                          web_contents_getter) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = web_contents_getter.Run();
    if (!web_contents)
      return;
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    AccountReconcilor* account_reconcilor =
        AccountReconcilorFactory::GetForProfile(profile);
    account_reconcilor_lock_.reset(
        new AccountReconcilor::Lock(account_reconcilor));
  }

 private:
  friend class base::RefCountedThreadSafe<AccountReconcilorLockWrapper>;
  ~AccountReconcilorLockWrapper() {}

  // The account reconcilor lock is created and deleted on UI thread.
  std::unique_ptr<AccountReconcilor::Lock,
                  content::BrowserThread::DeleteOnUIThread>
      account_reconcilor_lock_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorLockWrapper);
};

// The AccountReconcilor is suspended while a Dice request is in flight. This
// allows the DiceResponseHandler to see the response before the
// AccountReconcilor starts.
class DiceURLRequestUserData : public base::SupportsUserData::Data {
 public:
  // Attaches a DiceURLRequestUserData to the request if it needs to block the
  // AccountReconcilor.
  static void AttachToRequest(net::URLRequest* request) {
    if (!IsAccountConsistencyDiceEnabled())
      return;

    if (ShouldBlockReconcilorForRequest(request)) {
      const content::ResourceRequestInfo* info =
          content::ResourceRequestInfo::ForRequest(request);
      request->SetUserData(kDiceURLRequestUserDataKey,
                           base::MakeUnique<DiceURLRequestUserData>(
                               info->GetWebContentsGetterForRequest()));
    }
  }

  explicit DiceURLRequestUserData(
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter)
      : account_reconcilor_lock_wrapper_(new AccountReconcilorLockWrapper) {
    // The task takes a reference on the wrapper, because DiceRequestUserData
    // may be deleted before the task is run.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&AccountReconcilorLockWrapper::CreateLockOnUI,
                       account_reconcilor_lock_wrapper_, web_contents_getter));
  }

  // The Gaia cookie is received in one request, and the Dice response in
  // another request that is immediately following.
  // Start locking the reconcilor on the first request, and keep it locked for a
  // short time afterwards, to give the second request some time to start and
  // lock the reconcilor from there.
  ~DiceURLRequestUserData() override {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DiceURLRequestUserData::DoNothing,
                       account_reconcilor_lock_wrapper_),
        base::TimeDelta::FromMilliseconds(
            g_dice_account_reconcilor_blocked_delay_ms));
  }

 private:
  // Returns true if the account reconcilor needs be be blocked while a Gaia
  // sign-in request is in progress.
  //
  // The account reconcilor must be blocked on all request that may change the
  // Gaia authentication cookies. This includes:
  // * Main frame  requests.
  // * XHR requests having Gaia URL as referrer.
  static bool ShouldBlockReconcilorForRequest(net::URLRequest* request) {
    DCHECK(IsAccountConsistencyDiceEnabled());
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
    content::ResourceType resource_type = info->GetResourceType();

    if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
      return true;

    return (resource_type == content::RESOURCE_TYPE_XHR) &&
           gaia::IsGaiaSignonRealm(GURL(request->referrer()).GetOrigin());
  }
  // Dummy function used to extend the lifetime of the wrapper by keeping a
  // reference on it.
  static void DoNothing(scoped_refptr<AccountReconcilorLockWrapper> wrapper) {}

  scoped_refptr<AccountReconcilorLockWrapper> account_reconcilor_lock_wrapper_;
  DISALLOW_COPY_AND_ASSIGN(DiceURLRequestUserData);
};

// Processes the mirror response header on the UI thread. Currently depending
// on the value of |header_value|, it either shows the profile avatar menu, or
// opens an incognito window/tab.
void ProcessMirrorHeaderUIThread(
    ManageAccountsParams manage_accounts_params,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsAccountConsistencyMirrorEnabled());

  GAIAServiceType service_type = manage_accounts_params.service_type;
  DCHECK_NE(GAIA_SERVICE_TYPE_NONE, service_type);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  account_reconcilor->OnReceivedManageAccountsResponse(service_type);
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProcessDiceHeaderUIThread(
    const DiceResponseParams& dice_params,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsDiceFixAuthErrorsEnabled());

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(!profile->IsOffTheRecord());

  DiceResponseHandler* dice_response_handler =
      DiceResponseHandler::GetForProfile(profile);
  dice_response_handler->ProcessDiceHeader(
      dice_params,
      base::MakeUnique<ProcessDiceHeaderObserverImpl>(web_contents));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Looks for the X-Chrome-Manage-Accounts response header, and if found,
// tries to show the avatar bubble in the browser identified by the
// child/route id. Must be called on IO thread.
void ProcessMirrorResponseHeaderIfExists(net::URLRequest* request,
                                         bool is_off_the_record) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info || (info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME))
    return;

  if (!gaia::IsGaiaSignonRealm(request->url().GetOrigin()))
    return;

  net::HttpResponseHeaders* response_headers = request->response_headers();
  if (!response_headers)
    return;

  std::string header_value;
  if (!response_headers->GetNormalizedHeader(kChromeManageAccountsHeader,
                                             &header_value)) {
    return;
  }

  if (is_off_the_record) {
    NOTREACHED() << "Gaia should not send the X-Chrome-Manage-Accounts header "
                 << "in incognito.";
    return;
  }

  if (!IsAccountConsistencyMirrorEnabled()) {
    NOTREACHED() << "Gaia should not send the X-Chrome-Manage-Accounts header "
                 << "when Mirror is disabled.";
    return;
  }

  ManageAccountsParams params = BuildManageAccountsParams(header_value);
  // If the request does not have a response header or if the header contains
  // garbage, then |service_type| is set to |GAIA_SERVICE_TYPE_NONE|.
  if (params.service_type == GAIA_SERVICE_TYPE_NONE)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(ProcessMirrorHeaderUIThread, params,
                     info->GetWebContentsGetterForRequest()));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProcessDiceResponseHeaderIfExists(net::URLRequest* request,
                                       bool is_off_the_record) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (is_off_the_record)
    return;

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);

  if (!gaia::IsGaiaSignonRealm(request->url().GetOrigin()))
    return;

  if (!IsDiceFixAuthErrorsEnabled()) {
    return;
  }

  net::HttpResponseHeaders* response_headers = request->response_headers();
  if (!response_headers)
    return;

  std::string header_value;
  DiceResponseParams params;
  if (response_headers->GetNormalizedHeader(kDiceResponseHeader,
                                            &header_value)) {
    params = BuildDiceSigninResponseParams(header_value);
    // The header must be removed for privacy reasons, so that renderers never
    // have access to the authorization code.
    response_headers->RemoveHeader(kDiceResponseHeader);
  } else if (response_headers->GetNormalizedHeader(kGoogleSignoutResponseHeader,
                                                   &header_value)) {
    params = BuildDiceSignoutResponseParams(header_value);
  }

  // If the request does not have a response header or if the header contains
  // garbage, then |user_intention| is set to |NONE|.
  if (params.user_intention == DiceAction::NONE)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(ProcessDiceHeaderUIThread, params,
                 info->GetWebContentsGetterForRequest()));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

void SetDiceAccountReconcilorBlockDelayForTesting(int delay_ms) {
  g_dice_account_reconcilor_blocked_delay_ms = delay_ms;
}

void FixAccountConsistencyRequestHeader(net::URLRequest* request,
                                        const GURL& redirect_url,
                                        ProfileIOData* io_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (io_data->IsOffTheRecord())
    return;  // Account consistency is disabled in incognito.

  if (io_data->GetMainRequestContext() != request->context()) {
    // Account consistency requires the AccountReconcilor, which is only
    // attached to the main request context.
    // Note: InlineLoginUI uses an isolated request context and thus bypasses
    // the account consistency flow here. See http://crbug.com/428396
    return;
  }

  int profile_mode_mask = PROFILE_MODE_DEFAULT;
  if (io_data->incognito_availibility()->GetValue() ==
          IncognitoModePrefs::DISABLED ||
      IncognitoModePrefs::ArePlatformParentalControlsEnabled()) {
    profile_mode_mask |= PROFILE_MODE_INCOGNITO_DISABLED;
  }

  std::string account_id = io_data->google_services_account_id()->GetValue();

  // If new url is eligible to have the header, add it, otherwise remove it.

  // Dice header:
  bool dice_header_added = AppendOrRemoveDiceRequestHeader(
      request, redirect_url, account_id, io_data->IsSyncEnabled(),
      io_data->SyncHasAuthError(), io_data->GetCookieSettings());

  // Block the AccountReconcilor while the Dice requests are in flight. This
  // allows the DiceReponseHandler to process the response before the reconcilor
  // starts.
  if (dice_header_added)
    DiceURLRequestUserData::AttachToRequest(request);

  // Mirror header:
  AppendOrRemoveMirrorRequestHeader(request, redirect_url, account_id,
                                    io_data->GetCookieSettings(),
                                    profile_mode_mask);
}

void ProcessAccountConsistencyResponseHeaders(net::URLRequest* request,
                                              const GURL& redirect_url,
                                              bool is_off_the_record) {
  if (redirect_url.is_empty()) {
    // This is not a redirect.

    // See if the response contains the X-Chrome-Manage-Accounts header. If so
    // show the profile avatar bubble so that user can complete signin/out
    // action the native UI.
    ProcessMirrorResponseHeaderIfExists(request, is_off_the_record);
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Process the Dice header: on sign-in, exchange the authorization code for a
  // refresh token, on sign-out just follow the sign-out URL.
  ProcessDiceResponseHeaderIfExists(request, is_off_the_record);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

}  // namespace signin
