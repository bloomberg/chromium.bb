// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/investigator_dependency_provider.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_investigator.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void LogHistogramValue(int action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            signin_metrics::HISTOGRAM_MAX);
}

// Returns true if |profile| is a system profile or created from one.
bool IsSystemProfile(Profile* profile) {
  return profile->GetOriginalProfile()->IsSystemProfile();
}

void RedirectToNtpOrAppsPage(content::WebContents* contents,
                             signin_metrics::AccessPoint access_point) {
  // Do nothing if a navigation is pending, since this call can be triggered
  // from DidStartLoading. This avoids deleting the pending entry while we are
  // still navigating to it. See crbug/346632.
  if (contents->GetController().GetPendingEntry())
    return;

  VLOG(1) << "RedirectToNtpOrAppsPage";
  // Redirect to NTP/Apps page and display a confirmation bubble
  GURL url(access_point ==
                   signin_metrics::AccessPoint::ACCESS_POINT_APPS_PAGE_LINK
               ? chrome::kChromeUIAppsURL
               : chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params);
}

void RedirectToNtpOrAppsPageIfNecessary(
    content::WebContents* contents,
    signin_metrics::AccessPoint access_point) {
  if (access_point != signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS)
    RedirectToNtpOrAppsPage(contents, access_point);
}

void CloseModalSigninIfNeeded(InlineLoginHandlerImpl* handler) {
  if (handler && switches::UsePasswordSeparatedSigninFlow()) {
    Browser* browser = handler->GetDesktopBrowser();
    if (browser)
      browser->CloseModalSigninWindow();
  }
}

void UnlockProfileAndHideLoginUI(const base::FilePath profile_path,
                                 InlineLoginHandlerImpl* handler) {
  if (!profile_path.empty()) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    if (profile_manager) {
      ProfileAttributesEntry* entry;
      if (profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path, &entry)) {
        entry->SetIsSigninRequired(false);
      }
    }
  }
  if (handler)
    handler->web_ui()->CallJavascriptFunctionUnsafe("inline.login.closeDialog");
  UserManager::Hide();
}

bool IsCrossAccountError(Profile* profile,
                         const std::string& email,
                         const std::string& gaia_id) {
  InvestigatorDependencyProvider provider(profile);
  InvestigatedScenario scenario =
      SigninInvestigator(email, gaia_id, &provider).Investigate();

  return scenario == InvestigatedScenario::DIFFERENT_ACCOUNT;
}

}  // namespace

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    net::URLRequestContextGetter* getter,
    Profile* profile,
    Profile::CreateStatus create_status,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& session_index,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool choose_what_to_sync,
    bool confirm_untrusted_signin)
    : gaia_auth_fetcher_(this, GaiaConstants::kChromeSource, getter),
      handler_(handler),
      profile_(profile),
      create_status_(create_status),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      session_index_(session_index),
      auth_code_(auth_code),
      choose_what_to_sync_(choose_what_to_sync),
      confirm_untrusted_signin_(confirm_untrusted_signin) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  if (!auth_code_.empty()) {
    gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
        auth_code, signin_scoped_device_id);
  } else {
    DCHECK(!session_index_.empty());
    gaia_auth_fetcher_.StartCookieForOAuthLoginTokenExchangeWithDeviceId(
        session_index_, signin_scoped_device_id);
  }
}

InlineSigninHelper::~InlineSigninHelper() {}

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  if (signin::IsForceSigninEnabled()) {
    // With force sign in enabled, the browser window won't be opened until now.
    profiles::OpenBrowserWindowForProfile(
        base::Bind(&InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened,
                   base::Unretained(this), result),
        true, false, profile_, create_status_);
  } else {
    OnClientOAuthSuccessAndBrowserOpened(result, profile_, create_status_);
  }
}

void InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened(
    const ClientOAuthResult& result,
    Profile* profile,
    Profile::CreateStatus status) {
  if (signin::IsForceSigninEnabled())
    UnlockProfileAndHideLoginUI(profile_->GetPath(), handler_.get());
  content::WebContents* contents = NULL;
  Browser* browser = NULL;
  if (handler_) {
    contents = handler_->web_ui()->GetWebContents();
    browser = handler_->GetDesktopBrowser();
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Successful");

  // Prime the account tracker with this combination of gaia id/display email.
  std::string account_id =
      AccountTrackerServiceFactory::GetForProfile(profile_)
          ->SeedAccountInfo(gaia_id_, email_);

  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(current_url_);
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(current_url_);

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  std::string primary_email =
      signin_manager->GetAuthenticatedAccountInfo().email;
  if (gaia::AreEmailsSame(email_, primary_email) &&
      (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
       reason == signin_metrics::Reason::REASON_UNLOCK) &&
      switches::IsNewProfileManagement() && !password_.empty() &&
      profiles::IsLockAvailable(profile_)) {
    LocalAuth::SetLocalAuthCredentials(profile_, password_);
  }

  if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
      reason == signin_metrics::Reason::REASON_UNLOCK ||
      reason == signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
        UpdateCredentials(account_id, result.refresh_token);

    if (signin::IsAutoCloseEnabledInURL(current_url_)) {
      // Close the gaia sign in tab via a task to make sure we aren't in the
      // middle of any webui handler code.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&InlineLoginHandlerImpl::CloseTab, handler_,
                     signin::ShouldShowAccountManagement(current_url_)));
    }

    if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
        reason == signin_metrics::Reason::REASON_UNLOCK) {
      signin_manager->MergeSigninCredentialIntoCookieJar();
    }
    LogSigninReason(reason);
  } else {
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    SigninErrorController* error_controller =
        SigninErrorControllerFactory::GetForProfile(profile_);

    OneClickSigninSyncStarter::StartSyncMode start_mode =
        OneClickSigninSyncStarter::CONFIRM_SYNC_SETTINGS_FIRST;
    if (access_point == signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS ||
        choose_what_to_sync_) {
      bool show_settings_without_configure =
          error_controller->HasError() && sync_service &&
          sync_service->IsFirstSetupComplete();
      start_mode = show_settings_without_configure ?
          OneClickSigninSyncStarter::SHOW_SETTINGS_WITHOUT_CONFIGURE :
          OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST;
    }

    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required =
        confirm_untrusted_signin_ ?
            OneClickSigninSyncStarter::CONFIRM_UNTRUSTED_SIGNIN :
            OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN;

    bool start_signin = !HandleCrossAccountError(
        result.refresh_token, confirmation_required, start_mode);
    if (start_signin) {
      CreateSyncStarter(browser, contents, current_url_,
                        signin::GetNextPageURLForPromoURL(current_url_),
                        result.refresh_token, start_mode,
                        confirmation_required);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    }
  }
}

void InlineSigninHelper::CreateSyncStarter(
    Browser* browser,
    content::WebContents* contents,
    const GURL& current_url,
    const GURL& continue_url,
    const std::string& refresh_token,
    OneClickSigninSyncStarter::StartSyncMode start_mode,
    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required) {
  // OneClickSigninSyncStarter will delete itself once the job is done.
  new OneClickSigninSyncStarter(
      profile_, browser, gaia_id_, email_, password_, refresh_token, start_mode,
      contents, confirmation_required, current_url, continue_url,
      base::Bind(&InlineLoginHandlerImpl::SyncStarterCallback, handler_));
}

bool InlineSigninHelper::HandleCrossAccountError(
    const std::string& refresh_token,
    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required,
    OneClickSigninSyncStarter::StartSyncMode start_mode) {
  // With force sign in enabled, cross account sign in will be rejected in the
  // early stage so there is no need to show the warning page here.
  if (signin::IsForceSigninEnabled())
    return false;

  std::string last_email =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);

  // TODO(skym): Warn for high risk upgrade scenario, crbug.com/572754.
  if (!IsCrossAccountError(profile_, email_, gaia_id_))
    return false;

  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  SigninEmailConfirmationDialog::AskForConfirmation(
      web_contents, profile_, last_email, email_,
      base::Bind(&InlineSigninHelper::ConfirmEmailAction,
                 base::Unretained(this), web_contents, refresh_token,
                 confirmation_required, start_mode));
  return true;
}

void InlineSigninHelper::ConfirmEmailAction(
    content::WebContents* web_contents,
    const std::string& refresh_token,
    OneClickSigninSyncStarter::ConfirmationRequired confirmation_required,
    OneClickSigninSyncStarter::StartSyncMode start_mode,
    SigninEmailConfirmationDialog::Action action) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  switch (action) {
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      content::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      if (handler_) {
        handler_->SyncStarterCallback(
            OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
      }
      UserManager::Show(base::FilePath(), profiles::USER_MANAGER_NO_TUTORIAL,
                        profiles::USER_MANAGER_OPEN_CREATE_USER_PAGE);
      break;
    case SigninEmailConfirmationDialog::START_SYNC:
      content::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      CreateSyncStarter(browser, web_contents, current_url_, GURL(),
                        refresh_token, start_mode, confirmation_required);
      break;
    case SigninEmailConfirmationDialog::CLOSE:
      content::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      if (handler_) {
        handler_->SyncStarterCallback(
            OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
      }
      break;
    default:
      DCHECK(false) << "Invalid action";
  }
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void InlineSigninHelper::OnClientOAuthFailure(
  const GoogleServiceAuthError& error) {
  if (handler_)
    handler_->HandleLoginError(error.ToString(), base::string16());

  AboutSigninInternals* about_signin_internals =
    AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Failure");

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

InlineLoginHandlerImpl::InlineLoginHandlerImpl()
      : confirm_untrusted_signin_(false),
        weak_factory_(this) {
}

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() {}

// This method is not called with webview sign in enabled.
void InlineLoginHandlerImpl::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (!web_contents())
    return;

  // Returns early if this is not a gaia webview navigation.
  content::RenderFrameHost* gaia_frame =
      signin::GetAuthFrame(web_contents(), "signin-frame");
  if (render_frame_host != gaia_frame)
    return;

  // Loading any untrusted (e.g., HTTP) URLs in the privileged sign-in process
  // will require confirmation before the sign in takes effect.
  const GURL kGaiaExtOrigin(
      GaiaUrls::GetInstance()->signin_completed_continue_url().GetOrigin());
  if (!url.is_empty()) {
    GURL origin(url.GetOrigin());
    if (url.spec() != url::kAboutBlankURL &&
        origin != kGaiaExtOrigin &&
        !gaia::IsGaiaSignonRealm(origin)) {
      confirm_untrusted_signin_ = true;
    }
  }
}

// static
bool InlineLoginHandlerImpl::CanOffer(Profile* profile,
                                      CanOfferFor can_offer_for,
                                      const std::string& gaia_id,
                                      const std::string& email,
                                      std::string* error_message) {
  if (error_message)
    error_message->clear();

  if (!profile)
    return false;

  SigninManager* manager = SigninManagerFactory::GetForProfile(profile);
  if (manager && !manager->IsSigninAllowed())
    return false;

  if (!ChromeSigninClient::ProfileAllowsSigninCookies(profile))
    return false;

  if (!email.empty()) {
    if (!manager)
      return false;

    // Make sure this username is not prohibited by policy.
    if (!manager->IsAllowedUsername(email)) {
      if (error_message) {
        error_message->assign(
            l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED));
      }
      return false;
    }

    if (can_offer_for == CAN_OFFER_FOR_SECONDARY_ACCOUNT)
      return true;

    // If the signin manager already has an authenticated name, then this is a
    // re-auth scenario.  Make sure the email just signed in corresponds to
    // the one sign in manager expects.
    std::string current_email = manager->GetAuthenticatedAccountInfo().email;
    const bool same_email = gaia::AreEmailsSame(current_email, email);
    if (!current_email.empty() && !same_email) {
      UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                                signin_metrics::HISTOGRAM_ACCOUNT_MISSMATCH,
                                signin_metrics::HISTOGRAM_REAUTH_MAX);
      if (error_message) {
        error_message->assign(
            l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16(current_email)));
      }
      return false;
    }

    // If some profile, not just the current one, is already connected to this
    // account, don't show the infobar.
    if (g_browser_process && !same_email) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      if (profile_manager) {
        std::vector<ProfileAttributesEntry*> entries =
            profile_manager->GetProfileAttributesStorage().
                GetAllProfilesAttributes();

        for (const ProfileAttributesEntry* entry : entries) {
          // For backward compatibility, need to check also the username of the
          // profile, since the GAIA ID may not have been set yet in the
          // ProfileAttributesStorage.  It will be set once the profile
          // is opened.
          std::string profile_gaia_id = entry->GetGAIAId();
          std::string profile_email = base::UTF16ToUTF8(entry->GetUserName());
          if (gaia_id == profile_gaia_id ||
              gaia::AreEmailsSame(email, profile_email)) {
            if (error_message) {
              error_message->assign(
                  l10n_util::GetStringUTF8(IDS_SYNC_USER_NAME_IN_USE_ERROR));
            }
            return false;
          }
        }
      }
    }

    // With force sign in enabled, cross account sign in is not allowed.
    if (signin::IsForceSigninEnabled() &&
        IsCrossAccountError(profile, email, gaia_id)) {
      if (error_message) {
        std::string last_email =
            profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
        error_message->assign(l10n_util::GetStringFUTF8(
            IDS_SYNC_USED_PROFILE_ERROR, base::UTF8ToUTF16(last_email)));
      }
      return false;
    }
  }

  return true;
}

void InlineLoginHandlerImpl::SetExtraInitParams(base::DictionaryValue& params) {
  params.SetString("service", "chromiumsync");

  // If this was called from the user manager to reauthenticate the profile,
  // make sure the webui is aware.
  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSystemProfile(profile))
    params.SetBoolean("dontResizeNonEmbeddedPages", true);

  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(current_url);

  std::string is_constrained;
  net::GetValueForKeyInQuery(current_url, "constrained", &is_constrained);

  // Use new embedded flow if in constrained window.
  if (is_constrained == "1") {
    const bool is_new_gaia_flow = switches::UsePasswordSeparatedSigninFlow();
    const GURL& url = is_new_gaia_flow
            ? GaiaUrls::GetInstance()->embedded_signin_url()
            : GaiaUrls::GetInstance()->password_combined_embedded_signin_url();
    params.SetBoolean("isNewGaiaFlow", is_new_gaia_flow);
    params.SetString("clientId",
                     GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    params.SetString("gaiaPath", url.path().substr(1));

    std::string flow;
    switch (reason) {
      case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
        flow = "addaccount";
        break;
      case signin_metrics::Reason::REASON_REAUTHENTICATION:
      case signin_metrics::Reason::REASON_UNLOCK:
        flow = "reauth";
        break;
      default:
        flow = "signin";
        break;
    }
    params.SetString("flow", flow);
  }

  content::WebContentsObserver::Observe(contents);
  LogHistogramValue(signin_metrics::HISTOGRAM_SHOWN);
}

void InlineLoginHandlerImpl::CompleteLogin(const base::ListValue* args) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();

  const base::DictionaryValue* dict = NULL;
  args->GetDictionary(0, &dict);

  bool skip_for_now = false;
  dict->GetBoolean("skipForNow", &skip_for_now);
  if (skip_for_now) {
    signin::SetUserSkippedPromo(Profile::FromWebUI(web_ui()));
    SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
    return;
  }

  // This value exists only for webview sign in.
  bool trusted = false;
  if (dict->GetBoolean("trusted", &trusted))
    confirm_untrusted_signin_ = !trusted;

  base::string16 email_string16;
  dict->GetString("email", &email_string16);
  DCHECK(!email_string16.empty());
  std::string email(base::UTF16ToASCII(email_string16));

  base::string16 password_string16;
  dict->GetString("password", &password_string16);
  std::string password(base::UTF16ToASCII(password_string16));

  base::string16 gaia_id_string16;
  dict->GetString("gaiaId", &gaia_id_string16);
  DCHECK(!gaia_id_string16.empty());
  std::string gaia_id = base::UTF16ToASCII(gaia_id_string16);

  std::string is_constrained;
  net::GetValueForKeyInQuery(current_url, "constrained", &is_constrained);
  const bool is_password_separated_signin_flow = is_constrained == "1" &&
      switches::UsePasswordSeparatedSigninFlow();

  base::string16 session_index_string16;
  dict->GetString("sessionIndex", &session_index_string16);
  std::string session_index = base::UTF16ToASCII(session_index_string16);
  DCHECK(is_password_separated_signin_flow || !session_index.empty());

  base::string16 auth_code_string16;
  dict->GetString("authCode", &auth_code_string16);
  std::string auth_code = base::UTF16ToASCII(auth_code_string16);
  DCHECK(!is_password_separated_signin_flow || !auth_code.empty());

  bool choose_what_to_sync = false;
  dict->GetBoolean("chooseWhatToSync", &choose_what_to_sync);

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());

  // If this was called from the user manager to reauthenticate the profile,
  // the current profile is the system profile.  In this case, use the email to
  // find the right profile to reauthenticate.  Otherwise the profile can be
  // taken from web_ui().
  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSystemProfile(profile)) {
    ProfileManager* manager = g_browser_process->profile_manager();
    base::FilePath path = profiles::GetPathOfProfileWithEmail(manager, email);
    if (path.empty()) {
      path = UserManager::GetSigninProfilePath();
    }
    if (!path.empty()) {
      signin_metrics::Reason reason =
          signin::GetSigninReasonForPromoURL(current_url);
      // If we are only reauthenticating a profile in the user manager (and not
      // unlocking it), load the profile and finish the login.
      if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION) {
        FinishCompleteLoginParams params(
            this, partition, current_url, base::FilePath(),
            confirm_untrusted_signin_, email, gaia_id, password, session_index,
            auth_code, choose_what_to_sync);
        ProfileManager::CreateCallback callback =
            base::Bind(&InlineLoginHandlerImpl::FinishCompleteLogin, params);
        profiles::LoadProfileAsync(path, callback);
      } else {
        // Otherwise, switch to the profile and finish the login. Pass the
        // profile path so it can be marked as unlocked. Don't pass a handler
        // pointer since it will be destroyed before the callback runs.
        bool is_force_signin_enabled = signin::IsForceSigninEnabled();
        InlineLoginHandlerImpl* handler = nullptr;
        if (is_force_signin_enabled)
          handler = this;
        FinishCompleteLoginParams params(handler, partition, current_url, path,
                                         confirm_untrusted_signin_, email,
                                         gaia_id, password, session_index,
                                         auth_code, choose_what_to_sync);
        ProfileManager::CreateCallback callback =
            base::Bind(&InlineLoginHandlerImpl::FinishCompleteLogin, params);
        if (is_force_signin_enabled) {
          // Browser window will be opened after ClientOAuthSuccess.
          profiles::LoadProfileAsync(path, callback);
        } else {
          profiles::SwitchToProfile(path, true, callback,
                                    ProfileMetrics::SWITCH_PROFILE_UNLOCK);
        }
      }
    }
  } else {
    FinishCompleteLogin(
        FinishCompleteLoginParams(this, partition, current_url,
                                  base::FilePath(), confirm_untrusted_signin_,
                                  email, gaia_id, password, session_index,
                                  auth_code, choose_what_to_sync),
        profile,
        Profile::CREATE_STATUS_CREATED);
  }
}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    InlineLoginHandlerImpl* handler,
    content::StoragePartition* partition,
    const GURL& url,
    const base::FilePath& profile_path,
    bool confirm_untrusted_signin,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& session_index,
    const std::string& auth_code,
    bool choose_what_to_sync)
    : handler(handler),
      partition(partition),
      url(url),
      profile_path(profile_path),
      confirm_untrusted_signin(confirm_untrusted_signin),
      email(email),
      gaia_id(gaia_id),
      password(password),
      session_index(session_index),
      auth_code(auth_code),
      choose_what_to_sync(choose_what_to_sync) {}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    const FinishCompleteLoginParams& other) = default;

InlineLoginHandlerImpl::
    FinishCompleteLoginParams::~FinishCompleteLoginParams() {}

// static
void InlineLoginHandlerImpl::FinishCompleteLogin(
    const FinishCompleteLoginParams& params,
    Profile* profile,
    Profile::CreateStatus status) {
  // When doing a SAML sign in, this email check may result in a false
  // positive.  This happens when the user types one email address in the
  // gaia sign in page, but signs in to a different account in the SAML sign in
  // page.
  std::string default_email;
  std::string validate_email;
  if (net::GetValueForKeyInQuery(params.url, "email", &default_email) &&
      net::GetValueForKeyInQuery(params.url, "validateEmail",
                                 &validate_email) &&
      validate_email == "1" && !default_email.empty()) {
    if (!gaia::AreEmailsSame(params.email, default_email)) {
      if (params.handler) {
        params.handler->HandleLoginError(
            l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16(default_email)),
            base::UTF8ToUTF16(params.email));
      }
      return;
    }
  }

  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(params.url);
  signin_metrics::Reason reason =
      signin::GetSigninReasonForPromoURL(params.url);
  LogHistogramValue(signin_metrics::HISTOGRAM_ACCEPTED);
  bool switch_to_advanced =
      params.choose_what_to_sync &&
      (access_point != signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  LogHistogramValue(
      switch_to_advanced ? signin_metrics::HISTOGRAM_WITH_ADVANCED :
                           signin_metrics::HISTOGRAM_WITH_DEFAULTS);

  CanOfferFor can_offer_for = CAN_OFFER_FOR_ALL;
  switch (reason) {
    case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
      can_offer_for = CAN_OFFER_FOR_SECONDARY_ACCOUNT;
      break;
    case signin_metrics::Reason::REASON_REAUTHENTICATION:
    case signin_metrics::Reason::REASON_UNLOCK: {
      std::string primary_username =
          SigninManagerFactory::GetForProfile(profile)
              ->GetAuthenticatedAccountInfo()
              .email;
      if (!gaia::AreEmailsSame(default_email, primary_username))
        can_offer_for = CAN_OFFER_FOR_SECONDARY_ACCOUNT;
      break;
    }
    default:
      // No need to change |can_offer_for|.
      break;
  }

  std::string error_msg;
  bool can_offer = CanOffer(profile, can_offer_for, params.gaia_id,
                            params.email, &error_msg);
  if (!can_offer) {
    if (params.handler) {
      params.handler->HandleLoginError(error_msg,
                                       base::UTF8ToUTF16(params.email));
    }
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  about_signin_internals->OnAuthenticationResultReceived("Successful");

  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile);
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();
  base::WeakPtr<InlineLoginHandlerImpl> handler_weak_ptr;
  if (params.handler)
    handler_weak_ptr = params.handler->GetWeakPtr();

  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(
      handler_weak_ptr, params.partition->GetURLRequestContext(), profile,
      status, params.url, params.email, params.gaia_id, params.password,
      params.session_index, params.auth_code, signin_scoped_device_id,
      params.choose_what_to_sync, params.confirm_untrusted_signin);

  // If opened from user manager to unlock a profile, make sure the user manager
  // is closed and that the profile is marked as unlocked.
  if (!signin::IsForceSigninEnabled()) {
    UnlockProfileAndHideLoginUI(params.profile_path, params.handler);
  }
}

void InlineLoginHandlerImpl::HandleLoginError(const std::string& error_msg,
                                              const base::string16& email) {
  SyncStarterCallback(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
  Browser* browser = GetDesktopBrowser();
  Profile* profile = Profile::FromWebUI(web_ui());

  if (IsSystemProfile(profile))
    profile = g_browser_process->profile_manager()->GetProfileByPath(
        UserManager::GetSigninProfilePath());
  CloseModalSigninIfNeeded(this);
  if (!error_msg.empty()) {
    LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
        browser, base::UTF8ToUTF16(error_msg), email);
  }
}

Browser* InlineLoginHandlerImpl::GetDesktopBrowser() {
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui()));
  return browser;
}

void InlineLoginHandlerImpl::SyncStarterCallback(
    OneClickSigninSyncStarter::SyncSetupResult result) {
  content::WebContents* contents = web_ui()->GetWebContents();

  if (contents->GetController().GetPendingEntry()) {
    // Do nothing if a navigation is pending, since this call can be triggered
    // from DidStartLoading. This avoids deleting the pending entry while we are
    // still navigating to it. See crbug/346632.
    return;
  }

  const GURL& current_url = contents->GetLastCommittedURL();
  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForPromoURL(current_url);
  bool auto_close = signin::IsAutoCloseEnabledInURL(current_url);

  if (result == OneClickSigninSyncStarter::SYNC_SETUP_FAILURE) {
    RedirectToNtpOrAppsPage(contents, access_point);
  } else if (auto_close) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&InlineLoginHandlerImpl::CloseTab,
                   weak_factory_.GetWeakPtr(),
                   signin::ShouldShowAccountManagement(current_url)));
  } else {
    RedirectToNtpOrAppsPageIfNecessary(contents, access_point);
  }
}

void InlineLoginHandlerImpl::CloseTab(bool show_account_management) {
  content::WebContents* tab = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(tab);
      if (index != TabStripModel::kNoTab) {
        tab_strip_model->ExecuteContextMenuCommand(
            index, TabStripModel::CommandCloseTab);
      }
    }

    if (show_account_management) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
          signin::ManageAccountsParams(),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
    }
  }
}
