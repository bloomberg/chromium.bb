// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_helper.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_observer.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/sync/signin_histogram.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "components/google/core/browser/google_util.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_cookie_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"


namespace {

// ConfirmEmailDialogDelegate -------------------------------------------------

class ConfirmEmailDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  enum Action {
    CREATE_NEW_USER,
    START_SYNC,
    CLOSE
  };

  // Callback indicating action performed by the user.
  typedef base::Callback<void(Action)> Callback;

  // Ask the user for confirmation before starting to sync.
  static void AskForConfirmation(content::WebContents* contents,
                                 const std::string& last_email,
                                 const std::string& email,
                                 Callback callback);

 private:
  ConfirmEmailDialogDelegate(content::WebContents* contents,
                             const std::string& last_email,
                             const std::string& email,
                             Callback callback);
  virtual ~ConfirmEmailDialogDelegate();

  // TabModalConfirmDialogDelegate:
  virtual base::string16 GetTitle() OVERRIDE;
  virtual base::string16 GetDialogMessage() OVERRIDE;
  virtual base::string16 GetAcceptButtonTitle() OVERRIDE;
  virtual base::string16 GetCancelButtonTitle() OVERRIDE;
  virtual base::string16 GetLinkText() const OVERRIDE;
  virtual void OnAccepted() OVERRIDE;
  virtual void OnCanceled() OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual void OnLinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  std::string last_email_;
  std::string email_;
  Callback callback_;

  // Web contents from which the "Learn more" link should be opened.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmEmailDialogDelegate);
};

// static
void ConfirmEmailDialogDelegate::AskForConfirmation(
    content::WebContents* contents,
    const std::string& last_email,
    const std::string& email,
    Callback callback) {
  TabModalConfirmDialog::Create(
      new ConfirmEmailDialogDelegate(contents, last_email, email,
                                     callback), contents);
}

ConfirmEmailDialogDelegate::ConfirmEmailDialogDelegate(
    content::WebContents* contents,
    const std::string& last_email,
    const std::string& email,
    Callback callback)
  : TabModalConfirmDialogDelegate(contents),
    last_email_(last_email),
    email_(email),
    callback_(callback),
    web_contents_(contents) {
}

ConfirmEmailDialogDelegate::~ConfirmEmailDialogDelegate() {
}

base::string16 ConfirmEmailDialogDelegate::GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_TITLE);
}

base::string16 ConfirmEmailDialogDelegate::GetDialogMessage() {
  return l10n_util::GetStringFUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_MESSAGE,
      base::UTF8ToUTF16(last_email_), base::UTF8ToUTF16(email_));
}

base::string16 ConfirmEmailDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_OK_BUTTON);
}

base::string16 ConfirmEmailDialogDelegate::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_CANCEL_BUTTON);
}

base::string16 ConfirmEmailDialogDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

void ConfirmEmailDialogDelegate::OnAccepted() {
  base::ResetAndReturn(&callback_).Run(CREATE_NEW_USER);
}

void ConfirmEmailDialogDelegate::OnCanceled() {
  base::ResetAndReturn(&callback_).Run(START_SYNC);
}

void ConfirmEmailDialogDelegate::OnClosed() {
  base::ResetAndReturn(&callback_).Run(CLOSE);
}

void ConfirmEmailDialogDelegate::OnLinkClicked(
    WindowOpenDisposition disposition) {
  content::OpenURLParams params(
      GURL(chrome::kChromeSyncMergeTroubleshootingURL),
      content::Referrer(),
      NEW_POPUP,
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false);
  // It is guaranteed that |web_contents_| is valid here because when it's
  // deleted, the dialog is immediately closed and no further action can be
  // performed.
  web_contents_->OpenURL(params);
}


// Helpers --------------------------------------------------------------------

// Add a specific email to the list of emails rejected for one-click
// sign-in, for this profile.
void AddEmailToOneClickRejectedList(Profile* profile,
                                    const std::string& email) {
  ListPrefUpdate updater(profile->GetPrefs(),
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(new base::StringValue(email));
}

void LogOneClickHistogramValue(int action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.OneClickActions", action,
                            one_click_signin::HISTOGRAM_MAX);
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            one_click_signin::HISTOGRAM_MAX);
}

void RedirectToNtpOrAppsPageWithIds(int child_id,
                                    int route_id,
                                    signin::Source source) {
  content::WebContents* web_contents = tab_util::GetWebContentsByID(child_id,
                                                                    route_id);
  if (!web_contents)
    return;

  OneClickSigninHelper::RedirectToNtpOrAppsPage(web_contents, source);
}

// Start syncing with the given user information.
void StartSync(const OneClickSigninHelper::StartSyncArgs& args,
               OneClickSigninSyncStarter::StartSyncMode start_mode) {
  if (start_mode == OneClickSigninSyncStarter::UNDO_SYNC) {
    LogOneClickHistogramValue(one_click_signin::HISTOGRAM_UNDO);
    return;
  }

  // The wrapper deletes itself once it's done.
  OneClickSigninHelper::SyncStarterWrapper* wrapper =
      new OneClickSigninHelper::SyncStarterWrapper(args, start_mode);
  wrapper->Start();

  int action = one_click_signin::HISTOGRAM_MAX;
  switch (args.auto_accept) {
    case OneClickSigninHelper::AUTO_ACCEPT_EXPLICIT:
      break;
    case OneClickSigninHelper::AUTO_ACCEPT_ACCEPTED:
      action =
          start_mode == OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS ?
              one_click_signin::HISTOGRAM_AUTO_WITH_DEFAULTS :
              one_click_signin::HISTOGRAM_AUTO_WITH_ADVANCED;
      break;
    case OneClickSigninHelper::AUTO_ACCEPT_CONFIGURE:
      DCHECK(start_mode == OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
      action = one_click_signin::HISTOGRAM_AUTO_WITH_ADVANCED;
      break;
    default:
      NOTREACHED() << "Invalid auto_accept: " << args.auto_accept;
      break;
  }
  if (action != one_click_signin::HISTOGRAM_MAX)
    LogOneClickHistogramValue(action);
}

void StartExplicitSync(const OneClickSigninHelper::StartSyncArgs& args,
                       content::WebContents* contents,
                       OneClickSigninSyncStarter::StartSyncMode start_mode,
                       ConfirmEmailDialogDelegate::Action action) {
  bool enable_inline = !switches::IsEnableWebBasedSignin();
  if (action == ConfirmEmailDialogDelegate::START_SYNC) {
    StartSync(args, start_mode);
    if (!enable_inline) {
      // Redirect/tab closing for inline flow is handled by the sync callback.
      OneClickSigninHelper::RedirectToNtpOrAppsPageIfNecessary(
          contents, args.source);
    }
  } else {
    // Perform a redirection to the NTP/Apps page to hide the blank page when
    // the action is CLOSE or CREATE_NEW_USER. The redirection is useful when
    // the action is CREATE_NEW_USER because the "Create new user" page might
    // be opened in a different tab that is already showing settings.
    if (enable_inline) {
      // Redirect/tab closing for inline flow is handled by the sync callback.
      args.callback.Run(OneClickSigninSyncStarter::SYNC_SETUP_FAILURE);
    } else {
      // Don't redirect when the visible URL is not a blank page: if the
      // source is SOURCE_WEBSTORE_INSTALL, |contents| might be showing an app
      // page that shouldn't be hidden.
      //
      // If redirecting, don't do so immediately, otherwise there may be 2
      // nested navigations and a crash would occur (crbug.com/293261).  Post
      // the task to the current thread instead.
      if (signin::IsContinueUrlForWebBasedSigninFlow(
              contents->GetVisibleURL())) {
        base::MessageLoopProxy::current()->PostNonNestableTask(
            FROM_HERE,
            base::Bind(RedirectToNtpOrAppsPageWithIds,
                       contents->GetRenderProcessHost()->GetID(),
                       contents->GetRoutingID(),
                       args.source));
      }
    }
    if (action == ConfirmEmailDialogDelegate::CREATE_NEW_USER) {
      chrome::ShowSettingsSubPage(args.browser,
                                  std::string(chrome::kCreateProfileSubPage));
    }
  }
}

void ClearPendingEmailOnIOThread(content::ResourceContext* context) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  DCHECK(io_data);
  io_data->set_reverse_autologin_pending_email(std::string());
}

// Determines the source of the sign in and the continue URL.  It's either one
// of the known sign-in access points (first run, NTP, Apps page, menu, or
// settings) or it's an implicit sign in via another Google property.  In the
// former case, "service" is also checked to make sure its "chromiumsync".
signin::Source GetSigninSource(const GURL& url, GURL* continue_url) {
  DCHECK(url.is_valid());
  std::string value;
  net::GetValueForKeyInQuery(url, "service", &value);
  bool possibly_an_explicit_signin = value == "chromiumsync";

  // Find the final continue URL for this sign in.  In some cases, Gaia can
  // continue to itself, with the original continue URL buried under a couple
  // of layers of indirection.  Peel those layers away.  The final destination
  // can also be "IsGaiaSignonRealm" so stop if we get to the end (but be sure
  // we always extract at least one "continue" value).
  GURL local_continue_url = signin::GetNextPageURLForPromoURL(url);
  while (gaia::IsGaiaSignonRealm(local_continue_url.GetOrigin())) {
    GURL next_continue_url =
        signin::GetNextPageURLForPromoURL(local_continue_url);
    if (!next_continue_url.is_valid())
      break;
    local_continue_url = next_continue_url;
  }

  if (continue_url && local_continue_url.is_valid()) {
    DCHECK(!continue_url->is_valid() || *continue_url == local_continue_url);
    *continue_url = local_continue_url;
  }

  return possibly_an_explicit_signin ?
      signin::GetSourceForPromoURL(local_continue_url) :
      signin::SOURCE_UNKNOWN;
}

// Returns true if |url| is a valid URL that can occur during the sign in
// process.  Valid URLs are of the form:
//
//    https://accounts.google.{TLD}/...
//    https://accounts.youtube.com/...
//    https://accounts.blogger.com/...
//
// All special headers used by one click sign in occur on
// https://accounts.google.com URLs.  However, the sign in process may redirect
// to intermediate Gaia URLs that do not end with .com.  For example, an account
// that uses SMS 2-factor outside the US may redirect to country specific URLs.
//
// The sign in process may also redirect to youtube and blogger account URLs
// so that Gaia acts as a single signon service.
bool IsValidGaiaSigninRedirectOrResponseURL(const GURL& url) {
  std::string hostname = url.host();
  if (google_util::IsGoogleHostname(hostname, google_util::ALLOW_SUBDOMAIN)) {
    // Also using IsGaiaSignonRealm() to handle overriding with command line.
    return gaia::IsGaiaSignonRealm(url.GetOrigin()) ||
        StartsWithASCII(hostname, "accounts.", false);
  }

  GURL origin = url.GetOrigin();
  if (origin == GURL("https://accounts.youtube.com") ||
      origin == GURL("https://accounts.blogger.com"))
    return true;

  return false;
}

// Tells when we are in the process of showing either the signin to chrome page
// or the one click sign in to chrome page.
// NOTE: This should only be used for logging purposes since it relies on hard
// coded URLs that could change.
bool AreWeShowingSignin(GURL url, signin::Source source, std::string email) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  GURL clean_login_url =
      GaiaUrls::GetInstance()->service_login_url().ReplaceComponents(
          replacements);

  return (url.ReplaceComponents(replacements) == clean_login_url &&
          source != signin::SOURCE_UNKNOWN) ||
      (IsValidGaiaSigninRedirectOrResponseURL(url) &&
       url.spec().find("ChromeLoginPrompt") != std::string::npos &&
       !email.empty());
}

// If profile is valid then get signin scoped device id from signin client.
// Otherwise returns empty string.
std::string GetSigninScopedDeviceId(Profile* profile) {
  std::string signin_scoped_device_id;
  SigninClient* signin_client =
      profile ? ChromeSigninClientFactory::GetForProfile(profile) : NULL;
  if (signin_client) {
    signin_scoped_device_id = signin_client->GetSigninScopedDeviceId();
  }
  return signin_scoped_device_id;
}

// CurrentHistoryCleaner ------------------------------------------------------

// Watch a webcontents and remove URL from the history once loading is complete.
// We have to delay the cleaning until the new URL has finished loading because
// we're not allowed to remove the last-loaded URL from the history.  Objects
// of this type automatically self-destruct once they're finished their work.
class CurrentHistoryCleaner : public content::WebContentsObserver {
 public:
  explicit CurrentHistoryCleaner(content::WebContents* contents);
  virtual ~CurrentHistoryCleaner();

  // content::WebContentsObserver:
  virtual void WebContentsDestroyed() OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      content::PageTransition transition_type) OVERRIDE;

 private:
  scoped_ptr<content::WebContents> contents_;
  int history_index_to_remove_;

  DISALLOW_COPY_AND_ASSIGN(CurrentHistoryCleaner);
};

CurrentHistoryCleaner::CurrentHistoryCleaner(content::WebContents* contents)
    : WebContentsObserver(contents) {
  history_index_to_remove_ =
      web_contents()->GetController().GetLastCommittedEntryIndex();
}

CurrentHistoryCleaner::~CurrentHistoryCleaner() {
}

void CurrentHistoryCleaner::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    content::PageTransition transition_type) {
  // Return early if this is not top-level navigation.
  if (render_frame_host->GetParent())
    return;

  content::NavigationController* nc = &web_contents()->GetController();
  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      Profile::IMPLICIT_ACCESS);

  // Have to wait until something else gets added to history before removal.
  if (history_index_to_remove_ < nc->GetLastCommittedEntryIndex()) {
    content::NavigationEntry* entry =
        nc->GetEntryAtIndex(history_index_to_remove_);
    if (signin::IsContinueUrlForWebBasedSigninFlow(entry->GetURL())) {
      hs->DeleteURL(entry->GetURL());
      nc->RemoveEntryAtIndex(history_index_to_remove_);
      delete this;  // Success.
    }
  }
}

void CurrentHistoryCleaner::WebContentsDestroyed() {
  delete this;  // Failure.
}

}  // namespace


// StartSyncArgs --------------------------------------------------------------

OneClickSigninHelper::StartSyncArgs::StartSyncArgs()
    : profile(NULL),
      browser(NULL),
      auto_accept(AUTO_ACCEPT_NONE),
      web_contents(NULL),
      confirmation_required(OneClickSigninSyncStarter::NO_CONFIRMATION),
      source(signin::SOURCE_UNKNOWN) {}

OneClickSigninHelper::StartSyncArgs::StartSyncArgs(
    Profile* profile,
    Browser* browser,
    OneClickSigninHelper::AutoAccept auto_accept,
    const std::string& session_index,
    const std::string& email,
    const std::string& password,
    const std::string& refresh_token,
    content::WebContents* web_contents,
    bool untrusted_confirmation_required,
    signin::Source source,
    OneClickSigninSyncStarter::Callback callback)
    : profile(profile),
      browser(browser),
      auto_accept(auto_accept),
      session_index(session_index),
      email(email),
      password(password),
      refresh_token(refresh_token),
      web_contents(web_contents),
      source(source),
      callback(callback) {
  DCHECK(session_index.empty() != refresh_token.empty());
  if (untrusted_confirmation_required) {
    confirmation_required = OneClickSigninSyncStarter::CONFIRM_UNTRUSTED_SIGNIN;
  } else if (source == signin::SOURCE_SETTINGS ||
             source == signin::SOURCE_WEBSTORE_INSTALL) {
    // Do not display a status confirmation for webstore installs or re-auth.
    confirmation_required = OneClickSigninSyncStarter::NO_CONFIRMATION;
  } else {
    confirmation_required = OneClickSigninSyncStarter::CONFIRM_AFTER_SIGNIN;
  }
}

OneClickSigninHelper::StartSyncArgs::~StartSyncArgs() {}

// SyncStarterWrapper ---------------------------------------------------------

OneClickSigninHelper::SyncStarterWrapper::SyncStarterWrapper(
      const OneClickSigninHelper::StartSyncArgs& args,
      OneClickSigninSyncStarter::StartSyncMode start_mode)
    : args_(args), start_mode_(start_mode), weak_pointer_factory_(this) {
  BrowserList::AddObserver(this);

  // Cache the parent desktop for the browser, so we can reuse that same
  // desktop for any UI we want to display.
  desktop_type_ = args_.browser ? args_.browser->host_desktop_type()
                                : chrome::GetActiveDesktop();
}

OneClickSigninHelper::SyncStarterWrapper::~SyncStarterWrapper() {
  BrowserList::RemoveObserver(this);
}

void OneClickSigninHelper::SyncStarterWrapper::Start() {
  if (args_.refresh_token.empty()) {
    if (args_.password.empty()) {
      VerifyGaiaCookiesBeforeSignIn();
    } else {
      StartSigninOAuthHelper();
    }
  } else {
    OnSigninOAuthInformationAvailable(args_.email, args_.email,
                                      args_.refresh_token);
  }
}

void
OneClickSigninHelper::SyncStarterWrapper::OnSigninOAuthInformationAvailable(
    const std::string& email,
    const std::string& display_email,
    const std::string& refresh_token) {
  if (!gaia::AreEmailsSame(display_email, args_.email)) {
    DisplayErrorBubble(
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS).ToString());
  } else {
    StartOneClickSigninSyncStarter(email, refresh_token);
  }

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void OneClickSigninHelper::SyncStarterWrapper::OnSigninOAuthInformationFailure(
  const GoogleServiceAuthError& error) {
  DisplayErrorBubble(error.ToString());
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void OneClickSigninHelper::SyncStarterWrapper::OnBrowserRemoved(
    Browser* browser) {
  if (args_.browser == browser)
    args_.browser = NULL;
}

void OneClickSigninHelper::SyncStarterWrapper::VerifyGaiaCookiesBeforeSignIn() {
  scoped_refptr<SigninManagerCookieHelper> cookie_helper(
      new SigninManagerCookieHelper(
          args_.profile->GetRequestContext(),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::UI),
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO)));
  cookie_helper->StartFetchingGaiaCookiesOnUIThread(
      base::Bind(&SyncStarterWrapper::OnGaiaCookiesFetched,
                 weak_pointer_factory_.GetWeakPtr(),
                 args_.session_index));
}

void OneClickSigninHelper::SyncStarterWrapper::OnGaiaCookiesFetched(
    const std::string session_index, const net::CookieList& cookie_list) {
  net::CookieList::const_iterator it;
  bool success = false;
  for (it = cookie_list.begin(); it != cookie_list.end(); ++it) {
    // Make sure the LSID cookie is set on the GAIA host, instead of a super-
    // domain.
    if (it->Name() == "LSID") {
      if (it->IsHostCookie() && it->IsHttpOnly() && it->IsSecure()) {
        // Found a valid LSID cookie. Continue loop to make sure we don't have
        // invalid LSID cookies on any super-domain.
        success = true;
      } else {
        success = false;
        break;
      }
    }
  }

  if (success) {
    StartSigninOAuthHelper();
  } else {
    DisplayErrorBubble(
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS).ToString());
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void OneClickSigninHelper::SyncStarterWrapper::DisplayErrorBubble(
    const std::string& error_message) {
  args_.browser = OneClickSigninSyncStarter::EnsureBrowser(
      args_.browser, args_.profile, desktop_type_);
  args_.browser->window()->ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
      base::string16(),  // No email required - this is not a SAML confirmation.
      base::UTF8ToUTF16(error_message),
      // Callback is ignored.
      BrowserWindow::StartSyncCallback());
}

void OneClickSigninHelper::SyncStarterWrapper::StartSigninOAuthHelper() {
  std::string signin_scoped_device_id = GetSigninScopedDeviceId(args_.profile);
  signin_oauth_helper_.reset(
      new SigninOAuthHelper(args_.profile->GetRequestContext(),
                            args_.session_index,
                            signin_scoped_device_id,
                            this));
}

void
OneClickSigninHelper::SyncStarterWrapper::StartOneClickSigninSyncStarter(
    const std::string& email,
    const std::string& refresh_token) {
  // The starter deletes itself once it's done.
  new OneClickSigninSyncStarter(args_.profile, args_.browser,
                                email, args_.password,
                                refresh_token, start_mode_,
                                args_.web_contents,
                                args_.confirmation_required,
                                GURL(),
                                args_.callback);
}


// OneClickSigninHelper -------------------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(OneClickSigninHelper);

// static
const int OneClickSigninHelper::kMaxNavigationsSince = 10;

OneClickSigninHelper::OneClickSigninHelper(
    content::WebContents* web_contents,
    password_manager::PasswordManager* password_manager)
    : content::WebContentsObserver(web_contents),
      showing_signin_(false),
      auto_accept_(AUTO_ACCEPT_NONE),
      source_(signin::SOURCE_UNKNOWN),
      switched_to_advanced_(false),
      untrusted_navigations_since_signin_visit_(0),
      untrusted_confirmation_required_(false),
      do_not_clear_pending_email_(false),
      do_not_start_sync_for_testing_(false),
      weak_pointer_factory_(this) {
  // May be NULL during testing.
  if (password_manager) {
    password_manager->AddSubmissionCallback(
        base::Bind(&OneClickSigninHelper::PasswordSubmitted,
                   weak_pointer_factory_.GetWeakPtr()));
  }
}

OneClickSigninHelper::~OneClickSigninHelper() {}

// static
void OneClickSigninHelper::LogHistogramValue(
    signin::Source source, int action) {
  switch (source) {
    case signin::SOURCE_START_PAGE:
      UMA_HISTOGRAM_ENUMERATION("Signin.StartPageActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_NTP_LINK:
      UMA_HISTOGRAM_ENUMERATION("Signin.NTPLinkActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_MENU:
      UMA_HISTOGRAM_ENUMERATION("Signin.MenuActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_SETTINGS:
      UMA_HISTOGRAM_ENUMERATION("Signin.SettingsActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_EXTENSION_INSTALL_BUBBLE:
      UMA_HISTOGRAM_ENUMERATION("Signin.ExtensionInstallBubbleActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_WEBSTORE_INSTALL:
      UMA_HISTOGRAM_ENUMERATION("Signin.WebstoreInstallActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_APP_LAUNCHER:
      UMA_HISTOGRAM_ENUMERATION("Signin.AppLauncherActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_APPS_PAGE_LINK:
      UMA_HISTOGRAM_ENUMERATION("Signin.AppsPageLinkActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_BOOKMARK_BUBBLE:
      UMA_HISTOGRAM_ENUMERATION("Signin.BookmarkBubbleActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_AVATAR_BUBBLE_SIGN_IN:
      UMA_HISTOGRAM_ENUMERATION("Signin.AvatarBubbleActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT:
      UMA_HISTOGRAM_ENUMERATION("Signin.AvatarBubbleActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_DEVICES_PAGE:
      UMA_HISTOGRAM_ENUMERATION("Signin.DevicesPageActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    case signin::SOURCE_REAUTH:
      UMA_HISTOGRAM_ENUMERATION("Signin.ReauthActions", action,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    default:
      // This switch statement needs to be updated when the enum Source changes.
      COMPILE_ASSERT(signin::SOURCE_UNKNOWN == 13,
                     kSourceEnumHasChangedButNotThisSwitchStatement);
      UMA_HISTOGRAM_ENUMERATION("Signin.UnknownActions", action,
                                one_click_signin::HISTOGRAM_MAX);
  }
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            one_click_signin::HISTOGRAM_MAX);
}

// static
void OneClickSigninHelper::CreateForWebContentsWithPasswordManager(
    content::WebContents* contents,
    password_manager::PasswordManager* password_manager) {
  if (!FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(),
                          new OneClickSigninHelper(contents, password_manager));
  }
}

// static
bool OneClickSigninHelper::CanOffer(content::WebContents* web_contents,
                                    CanOfferFor can_offer_for,
                                    const std::string& email,
                                    std::string* error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    VLOG(1) << "OneClickSigninHelper::CanOffer";

  if (error_message)
    error_message->clear();

  if (!web_contents)
    return false;

  if (web_contents->GetBrowserContext()->IsOffTheRecord())
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return false;

  SigninManager* manager =
      SigninManagerFactory::GetForProfile(profile);
  if (manager && !manager->IsSigninAllowed())
    return false;

  if (can_offer_for == CAN_OFFER_FOR_INTERSTITAL_ONLY &&
      !profile->GetPrefs()->GetBoolean(prefs::kReverseAutologinEnabled))
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

    if (can_offer_for != CAN_OFFER_FOR_SECONDARY_ACCOUNT) {
      // If the signin manager already has an authenticated name, then this is a
      // re-auth scenario.  Make sure the email just signed in corresponds to
      // the one sign in manager expects.
      std::string current_email = manager->GetAuthenticatedUsername();
      const bool same_email = gaia::AreEmailsSame(current_email, email);
      if (!current_email.empty() && !same_email) {
        UMA_HISTOGRAM_ENUMERATION("Signin.Reauth",
                                  signin::HISTOGRAM_ACCOUNT_MISSMATCH,
                                  signin::HISTOGRAM_MAX);
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
        ProfileManager* manager = g_browser_process->profile_manager();
        if (manager) {
          ProfileInfoCache& cache = manager->GetProfileInfoCache();
          for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
            std::string current_email =
                base::UTF16ToUTF8(cache.GetUserNameOfProfileAtIndex(i));
            if (gaia::AreEmailsSame(email, current_email)) {
              if (error_message) {
                error_message->assign(
                    l10n_util::GetStringUTF8(IDS_SYNC_USER_NAME_IN_USE_ERROR));
              }
              return false;
            }
          }
        }
      }
    }

    // If email was already rejected by this profile for one-click sign-in.
    if (can_offer_for == CAN_OFFER_FOR_INTERSTITAL_ONLY) {
      const base::ListValue* rejected_emails = profile->GetPrefs()->GetList(
          prefs::kReverseAutologinRejectedEmailList);
      if (!rejected_emails->empty()) {
        base::ListValue::const_iterator iter = rejected_emails->Find(
            base::StringValue(email));
        if (iter != rejected_emails->end())
          return false;
      }
    }
  }

  VLOG(1) << "OneClickSigninHelper::CanOffer: yes we can";
  return true;
}

// static
OneClickSigninHelper::Offer OneClickSigninHelper::CanOfferOnIOThread(
    net::URLRequest* request,
    ProfileIOData* io_data) {
  return CanOfferOnIOThreadImpl(request->url(), request, io_data);
}

// static
OneClickSigninHelper::Offer OneClickSigninHelper::CanOfferOnIOThreadImpl(
    const GURL& url,
    base::SupportsUserData* request,
    ProfileIOData* io_data) {
  if (!gaia::IsGaiaSignonRealm(url.GetOrigin()))
    return IGNORE_REQUEST;

  if (!io_data)
    return DONT_OFFER;

  // Check for incognito before other parts of the io_data, since those
  // members may not be initalized.
  if (io_data->IsOffTheRecord())
    return DONT_OFFER;

  if (!io_data->signin_allowed()->GetValue())
    return DONT_OFFER;

  if (!io_data->reverse_autologin_enabled()->GetValue())
    return DONT_OFFER;

  if (!io_data->google_services_username()->GetValue().empty())
    return DONT_OFFER;

  if (!ChromeSigninClient::SettingsAllowSigninCookies(
          io_data->GetCookieSettings()))
    return DONT_OFFER;

  // The checks below depend on chrome already knowing what account the user
  // signed in with.  This happens only after receiving the response containing
  // the Google-Accounts-SignIn header.  Until then, if there is even a chance
  // that we want to connect the profile, chrome needs to tell Gaia that
  // it should offer the interstitial.  Therefore missing one click data on
  // the request means can offer is true.
  const std::string& pending_email = io_data->reverse_autologin_pending_email();
  if (!pending_email.empty()) {
    if (!SigninManager::IsUsernameAllowedByPolicy(pending_email,
            io_data->google_services_username_pattern()->GetValue())) {
      return DONT_OFFER;
    }

    std::vector<std::string> rejected_emails =
        io_data->one_click_signin_rejected_email_list()->GetValue();
    if (std::count_if(rejected_emails.begin(), rejected_emails.end(),
                      std::bind2nd(std::equal_to<std::string>(),
                                   pending_email)) > 0) {
      return DONT_OFFER;
    }

    if (io_data->signin_names()->GetEmails().count(
            base::UTF8ToUTF16(pending_email)) > 0) {
      return DONT_OFFER;
    }
  }

  return CAN_OFFER;
}

// static
void OneClickSigninHelper::ShowInfoBarIfPossible(net::URLRequest* request,
                                                 ProfileIOData* io_data,
                                                 int child_id,
                                                 int route_id) {
  std::string google_chrome_signin_value;
  std::string google_accounts_signin_value;
  request->GetResponseHeaderByName("Google-Chrome-SignIn",
                                   &google_chrome_signin_value);
  request->GetResponseHeaderByName("Google-Accounts-SignIn",
                                   &google_accounts_signin_value);

  if (!google_accounts_signin_value.empty() ||
      !google_chrome_signin_value.empty()) {
    VLOG(1) << "OneClickSigninHelper::ShowInfoBarIfPossible:"
            << " g-a-s='" << google_accounts_signin_value << "'"
            << " g-c-s='" << google_chrome_signin_value << "'";
  }

  if (!gaia::IsGaiaSignonRealm(request->url().GetOrigin()))
    return;

  // Parse Google-Accounts-SignIn.
  std::vector<std::pair<std::string, std::string> > pairs;
  base::SplitStringIntoKeyValuePairs(google_accounts_signin_value, '=', ',',
                                     &pairs);
  std::string session_index;
  std::string email;
  for (size_t i = 0; i < pairs.size(); ++i) {
    const std::pair<std::string, std::string>& pair = pairs[i];
    const std::string& key = pair.first;
    const std::string& value = pair.second;
    if (key == "email") {
      base::TrimString(value, "\"", &email);
    } else if (key == "sessionindex") {
      session_index = value;
    }
  }

  // Later in the chain of this request, we'll need to check the email address
  // in the IO thread (see CanOfferOnIOThread).  So save the email address as
  // user data on the request (only for web-based flow).
  if (!email.empty())
    io_data->set_reverse_autologin_pending_email(email);

  if (!email.empty() || !session_index.empty()) {
    VLOG(1) << "OneClickSigninHelper::ShowInfoBarIfPossible:"
            << " email=" << email
            << " sessionindex=" << session_index;
  }

  // Parse Google-Chrome-SignIn.
  AutoAccept auto_accept = AUTO_ACCEPT_NONE;
  signin::Source source = signin::SOURCE_UNKNOWN;
  GURL continue_url;
  std::vector<std::string> tokens;
  base::SplitString(google_chrome_signin_value, ',', &tokens);
  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& token = tokens[i];
    if (token == "accepted") {
      auto_accept = AUTO_ACCEPT_ACCEPTED;
    } else if (token == "configure") {
      auto_accept = AUTO_ACCEPT_CONFIGURE;
    } else if (token == "rejected-for-profile") {
      auto_accept = AUTO_ACCEPT_REJECTED_FOR_PROFILE;
    }
  }

  // If this is an explicit sign in (i.e., first run, NTP, Apps page, menu,
  // settings) then force the auto accept type to explicit.
  source = GetSigninSource(request->url(), &continue_url);
  if (source != signin::SOURCE_UNKNOWN)
    auto_accept = AUTO_ACCEPT_EXPLICIT;

  if (auto_accept != AUTO_ACCEPT_NONE) {
    VLOG(1) << "OneClickSigninHelper::ShowInfoBarIfPossible:"
            << " auto_accept=" << auto_accept;
  }

  // If |session_index|, |email|, |auto_accept|, and |continue_url| all have
  // their default value, don't bother posting a task to the UI thread.
  // It will be a noop anyway.
  //
  // The two headers above may (but not always) come in different http requests
  // so a post to the UI thread is still needed if |auto_accept| is not its
  // default value, but |email| and |session_index| are.
  if (session_index.empty() && email.empty() &&
      auto_accept == AUTO_ACCEPT_NONE && !continue_url.is_valid()) {
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OneClickSigninHelper::ShowInfoBarUIThread, session_index,
                 email, auto_accept, source, continue_url, child_id, route_id));
}

// static
void OneClickSigninHelper::LogConfirmHistogramValue(int action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.OneClickConfirmation", action,
                            one_click_signin::HISTOGRAM_CONFIRM_MAX);
}
// static
void OneClickSigninHelper::ShowInfoBarUIThread(
    const std::string& session_index,
    const std::string& email,
    AutoAccept auto_accept,
    signin::Source source,
    const GURL& continue_url,
    int child_id,
    int route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = tab_util::GetWebContentsByID(child_id,
                                                                    route_id);
  if (!web_contents)
    return;

  // TODO(mathp): The appearance of this infobar should be tested using a
  // browser_test.
  OneClickSigninHelper* helper =
      OneClickSigninHelper::FromWebContents(web_contents);
  if (!helper)
    return;

  if (auto_accept != AUTO_ACCEPT_NONE)
    helper->auto_accept_ = auto_accept;

  if (source != signin::SOURCE_UNKNOWN &&
      helper->source_ == signin::SOURCE_UNKNOWN) {
    helper->source_ = source;
  }

  // Save the email in the one-click signin manager.  The manager may
  // not exist if the contents is incognito or if the profile is already
  // connected to a Google account.
  if (!session_index.empty())
    helper->session_index_ = session_index;

  if (!email.empty())
    helper->email_ = email;

  CanOfferFor can_offer_for =
      (auto_accept != AUTO_ACCEPT_EXPLICIT &&
          helper->auto_accept_ != AUTO_ACCEPT_EXPLICIT) ?
          CAN_OFFER_FOR_INTERSTITAL_ONLY : CAN_OFFER_FOR_ALL;

  std::string error_message;

  if (!web_contents || !CanOffer(web_contents, can_offer_for, email,
                                 &error_message)) {
    VLOG(1) << "OneClickSigninHelper::ShowInfoBarUIThread: not offering";
    // TODO(rogerta): Can we just display our error now instead of keeping it
    // around and doing it later?
    if (helper && helper->error_message_.empty() && !error_message.empty())
      helper->error_message_ = error_message;

    return;
  }

  // Only allow the dedicated signin process to sign the user into
  // Chrome without intervention, because it doesn't load any untrusted
  // pages.  If at any point an untrusted page is detected, chrome will
  // show a modal dialog asking the user to confirm.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SigninClient* signin_client =
      profile ? ChromeSigninClientFactory::GetForProfile(profile) : NULL;
  helper->untrusted_confirmation_required_ |=
      (signin_client && !signin_client->IsSigninProcess(child_id));

  if (continue_url.is_valid()) {
    // Set |original_continue_url_| if it is currently empty. |continue_url|
    // could be modified by gaia pages, thus we need to record the original
    // continue url to navigate back to the right page when sync setup is
    // complete.
    if (helper->original_continue_url_.is_empty())
      helper->original_continue_url_ = continue_url;
    helper->continue_url_ = continue_url;
  }
}

// static
void OneClickSigninHelper::RemoveSigninRedirectURLHistoryItem(
    content::WebContents* web_contents) {
  // Only actually remove the item if it's the blank.html continue url.
  if (signin::IsContinueUrlForWebBasedSigninFlow(
          web_contents->GetLastCommittedURL())) {
    new CurrentHistoryCleaner(web_contents);  // will self-destruct when done
  }
}

// static
void OneClickSigninHelper::ShowSigninErrorBubble(Browser* browser,
                                                 const std::string& error) {
  DCHECK(!error.empty());

  browser->window()->ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
      base::string16(), /* no SAML email */
      base::UTF8ToUTF16(error),
      // This callback is never invoked.
      // TODO(rogerta): Separate out the bubble API so we don't have to pass
      // ignored |email| and |callback| params.
      BrowserWindow::StartSyncCallback());
}

// static
bool OneClickSigninHelper::HandleCrossAccountError(
    Profile* profile,
    const std::string& session_index,
    const std::string& email,
    const std::string& password,
    const std::string& refresh_token,
    OneClickSigninHelper::AutoAccept auto_accept,
    signin::Source source,
    OneClickSigninSyncStarter::StartSyncMode start_mode,
    OneClickSigninSyncStarter::Callback sync_callback) {
  std::string last_email =
      profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);

  if (!last_email.empty() && !gaia::AreEmailsSame(last_email, email)) {
    // If the new email address is different from the email address that
    // just signed in, show a confirmation dialog on top of the current active
    // tab.

    // No need to display a second confirmation so pass false below.
    // TODO(atwilson): Move this into OneClickSigninSyncStarter.
    // The tab modal dialog always executes its callback before |contents|
    // is deleted.
    Browser* browser = chrome::FindLastActiveWithProfile(
        profile, chrome::GetActiveDesktop());
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    ConfirmEmailDialogDelegate::AskForConfirmation(
        contents,
        last_email,
        email,
        base::Bind(
            &StartExplicitSync,
            StartSyncArgs(profile, browser, auto_accept,
                          session_index, email, password,
                          refresh_token,
                          contents, false /* confirmation_required */, source,
                          sync_callback),
            contents,
            start_mode));
    return true;
  }

  return false;
}

// static
void OneClickSigninHelper::RedirectToNtpOrAppsPage(
    content::WebContents* contents, signin::Source source) {
  // Do nothing if a navigation is pending, since this call can be triggered
  // from DidStartLoading. This avoids deleting the pending entry while we are
  // still navigating to it. See crbug/346632.
  if (contents->GetController().GetPendingEntry())
    return;

  VLOG(1) << "RedirectToNtpOrAppsPage";
  // Redirect to NTP/Apps page and display a confirmation bubble
  GURL url(source == signin::SOURCE_APPS_PAGE_LINK ?
           chrome::kChromeUIAppsURL : chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url,
                                content::Referrer(),
                                CURRENT_TAB,
                                content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                false);
  contents->OpenURL(params);
}

// static
void OneClickSigninHelper::RedirectToNtpOrAppsPageIfNecessary(
    content::WebContents* contents, signin::Source source) {
  if (source != signin::SOURCE_SETTINGS &&
      source != signin::SOURCE_WEBSTORE_INSTALL) {
    RedirectToNtpOrAppsPage(contents, source);
  }
}

void OneClickSigninHelper::RedirectToSignin() {
  VLOG(1) << "OneClickSigninHelper::RedirectToSignin";

  // Extract the existing sounce=X value.  Default to "2" if missing.
  signin::Source source = signin::GetSourceForPromoURL(continue_url_);
  if (source == signin::SOURCE_UNKNOWN)
    source = signin::SOURCE_MENU;
  GURL page = signin::GetPromoURL(source, false);

  content::WebContents* contents = web_contents();
  contents->GetController().LoadURL(page,
                                    content::Referrer(),
                                    content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
}

void OneClickSigninHelper::CleanTransientState() {
  VLOG(1) << "OneClickSigninHelper::CleanTransientState";
  showing_signin_ = false;
  email_.clear();
  password_.clear();
  auto_accept_ = AUTO_ACCEPT_NONE;
  source_ = signin::SOURCE_UNKNOWN;
  switched_to_advanced_ = false;
  continue_url_ = GURL();
  untrusted_navigations_since_signin_visit_ = 0;
  untrusted_confirmation_required_ = false;
  error_message_.clear();

  // Post to IO thread to clear pending email.
  if (!do_not_clear_pending_email_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearPendingEmailOnIOThread,
                   base::Unretained(profile->GetResourceContext())));
  }
}

void OneClickSigninHelper::PasswordSubmitted(
    const autofill::PasswordForm& form) {
  // We only need to scrape the password for Gaia logins.
  if (gaia::IsGaiaSignonRealm(GURL(form.signon_realm))) {
    VLOG(1) << "OneClickSigninHelper::DidNavigateAnyFrame: got password";
    password_ = base::UTF16ToUTF8(form.password_value);
  }
}

void OneClickSigninHelper::SetDoNotClearPendingEmailForTesting() {
  do_not_clear_pending_email_ = true;
}

void OneClickSigninHelper::set_do_not_start_sync_for_testing() {
  do_not_start_sync_for_testing_ = true;
}

void OneClickSigninHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  VLOG(1) << "OneClickSigninHelper::DidStartNavigationToPendingEntry: url=" <<
      url.spec();
  // If the tab navigates to a new page, and this page is not a valid Gaia
  // sign in redirect or reponse, or the expected continue URL, make sure to
  // clear the internal state.  This is needed to detect navigations in the
  // middle of the sign in process that may redirect back to the sign in
  // process (see crbug.com/181163 for details).
  GURL::Replacements replacements;
  replacements.ClearQuery();

  if (!IsValidGaiaSigninRedirectOrResponseURL(url) &&
      continue_url_.is_valid() &&
      url.ReplaceComponents(replacements) !=
          continue_url_.ReplaceComponents(replacements)) {
    if (++untrusted_navigations_since_signin_visit_ > kMaxNavigationsSince)
      CleanTransientState();
  }
}

void OneClickSigninHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!SigninManager::IsWebBasedSigninFlowURL(params.url)) {
    // Make sure the renderer process is no longer considered the trusted
    // sign-in process when a navigation to a non-sign-in URL occurs.
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    SigninClient* signin_client =
        profile ? ChromeSigninClientFactory::GetForProfile(profile) : NULL;
    int process_id = web_contents()->GetRenderProcessHost()->GetID();
    if (signin_client && signin_client->IsSigninProcess(process_id))
      signin_client->ClearSigninProcess();

    // If the navigation to a non-sign-in URL hasn't been triggered by the web
    // contents, the sign in flow has been aborted and the state must be
    // cleaned (crbug.com/269421).
    if (!content::PageTransitionIsWebTriggerable(params.transition) &&
        auto_accept_ != AUTO_ACCEPT_NONE) {
      CleanTransientState();
    }
  }
}

void OneClickSigninHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  // If the user left the sign in process, clear all members.
  // TODO(rogerta): might need to allow some youtube URLs.
  content::WebContents* contents = web_contents();
  const GURL url = contents->GetLastCommittedURL();
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  VLOG(1) << "OneClickSigninHelper::DidStopLoading: url=" << url.spec();

  if (url.scheme() == content::kChromeUIScheme) {
    // Suppresses OneClickSigninHelper on webUI pages to avoid inteference with
    // inline signin flows.
    VLOG(1) << "OneClickSigninHelper::DidStopLoading: suppressed for url="
            << url.spec();
    CleanTransientState();
    return;
  }

  // If an error has already occured during the sign in flow, make sure to
  // display it to the user and abort the process.  Do this only for
  // explicit sign ins.
  // TODO(rogerta): Could we move this code back up to ShowInfoBarUIThread()?
  if (!error_message_.empty() && auto_accept_ == AUTO_ACCEPT_EXPLICIT) {
    VLOG(1) << "OneClickSigninHelper::DidStopLoading: error=" << error_message_;
    RemoveSigninRedirectURLHistoryItem(contents);
    // After we redirect to NTP, our browser pointer gets corrupted because the
    // WebContents have changed, so grab the browser pointer
    // before the navigation.
    Browser* browser = chrome::FindBrowserWithWebContents(contents);

    // Redirect to the landing page and display an error popup.
    RedirectToNtpOrAppsPage(web_contents(), source_);
    ShowSigninErrorBubble(browser, error_message_);
    CleanTransientState();
    return;
  }

  if (AreWeShowingSignin(url, source_, email_)) {
    if (!showing_signin_) {
      if (source_ == signin::SOURCE_UNKNOWN)
        LogOneClickHistogramValue(one_click_signin::HISTOGRAM_SHOWN);
      else
        LogHistogramValue(source_, one_click_signin::HISTOGRAM_SHOWN);
    }
    showing_signin_ = true;
  }

  // When Gaia finally redirects to the continue URL, Gaia will add some
  // extra query parameters.  So ignore the parameters when checking to see
  // if the user has continued.  Sometimes locales will redirect to a country-
  // specific TLD so just make sure it's a valid domain instead of comparing
  // for an exact match.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  bool google_domain_url = google_util::IsGoogleDomainUrl(
      url,
      google_util::ALLOW_SUBDOMAIN,
      google_util::DISALLOW_NON_STANDARD_PORTS);
  const bool continue_url_match =
      google_domain_url &&
      url.ReplaceComponents(replacements).path() ==
        continue_url_.ReplaceComponents(replacements).path();
  const bool original_continue_url_match =
      google_domain_url &&
      url.ReplaceComponents(replacements).path() ==
        original_continue_url_.ReplaceComponents(replacements).path();

  if (continue_url_match)
    RemoveSigninRedirectURLHistoryItem(contents);

  // If there is no valid email yet, there is nothing to do.  As of M26, the
  // password is allowed to be empty, since its no longer required to setup
  // sync.
  if (email_.empty()) {
    VLOG(1) << "OneClickSigninHelper::DidStopLoading: nothing to do";
    // Original-url check done because some user actions cans get us to a page
    // via a POST instead of a GET (and thus to immediate "cuntinue url") but
    // we still want redirects from the "blank.html" landing page to work for
    // non-security related redirects like NTP.
    // https://code.google.com/p/chromium/issues/detail?id=321938
    if (original_continue_url_match) {
      if (auto_accept_ == AUTO_ACCEPT_EXPLICIT)
        RedirectToSignin();
      std::string unused_value;
      if (net::GetValueForKeyInQuery(url, "ntp", &unused_value)) {
        signin::SetUserSkippedPromo(profile);
        RedirectToNtpOrAppsPage(web_contents(), source_);
      }
    } else {
      if (!IsValidGaiaSigninRedirectOrResponseURL(url) &&
          ++untrusted_navigations_since_signin_visit_ > kMaxNavigationsSince) {
        CleanTransientState();
      }
    }

    return;
  }

  if (!continue_url_match && IsValidGaiaSigninRedirectOrResponseURL(url))
    return;

  // During an explicit sign in, if the user has not yet reached the final
  // continue URL, wait for it to arrive. Note that Gaia will add some extra
  // query parameters to the continue URL.  Ignore them when checking to
  // see if the user has continued.
  //
  // If this is not an explicit sign in, we don't need to check if we landed
  // on the right continue URL.  This is important because the continue URL
  // may itself lead to a redirect, which means this function will never see
  // the continue URL go by.
  if (auto_accept_ == AUTO_ACCEPT_EXPLICIT) {
    DCHECK(source_ != signin::SOURCE_UNKNOWN);
    if (!continue_url_match) {
      VLOG(1) << "OneClickSigninHelper::DidStopLoading: invalid url='"
              << url.spec()
              << "' expected continue url=" << continue_url_;
      CleanTransientState();
      return;
    }

    // In explicit sign ins, the user may have changed the box
    // "Let me choose what to sync".  This is reflected as a change in the
    // source of the continue URL.  Make one last check of the current URL
    // to see if there is a valid source.  If so, it overrides the
    // current source.
    //
    // If the source was changed to SOURCE_SETTINGS, we want
    // OneClickSigninSyncStarter to reuse the current tab to display the
    // advanced configuration.
    signin::Source source = signin::GetSourceForPromoURL(url);
    if (source != source_) {
      source_ = source;
      switched_to_advanced_ = source == signin::SOURCE_SETTINGS;
    }
  }

  Browser* browser = chrome::FindBrowserWithWebContents(contents);

  VLOG(1) << "OneClickSigninHelper::DidStopLoading: signin is go."
          << " auto_accept=" << auto_accept_
          << " source=" << source_;

  switch (auto_accept_) {
    case AUTO_ACCEPT_NONE:
      if (showing_signin_)
        LogOneClickHistogramValue(one_click_signin::HISTOGRAM_DISMISSED);
      break;
    case AUTO_ACCEPT_ACCEPTED:
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_ACCEPTED);
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_WITH_DEFAULTS);
      SigninManager::DisableOneClickSignIn(profile->GetPrefs());
      // Start syncing with the default settings - prompt the user to sign in
      // first.
      if (!do_not_start_sync_for_testing_) {
        StartSync(
            StartSyncArgs(profile, browser, auto_accept_,
                          session_index_, email_, password_, "",
                          NULL  /* don't force sync setup in same tab */,
                          true  /* confirmation_required */, source_,
                          CreateSyncStarterCallback()),
            OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
      }
      break;
    case AUTO_ACCEPT_CONFIGURE:
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_ACCEPTED);
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_WITH_ADVANCED);
      SigninManager::DisableOneClickSignIn(profile->GetPrefs());
      // Display the extra confirmation (even in the SAML case) in case this
      // was an untrusted renderer.
      if (!do_not_start_sync_for_testing_) {
        StartSync(
            StartSyncArgs(profile, browser, auto_accept_,
                          session_index_, email_, password_, "",
                          NULL  /* don't force sync setup in same tab */,
                          true  /* confirmation_required */, source_,
                          CreateSyncStarterCallback()),
            OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
      }
      break;
    case AUTO_ACCEPT_EXPLICIT: {
      signin::Source original_source =
          signin::GetSourceForPromoURL(original_continue_url_);
      if (switched_to_advanced_) {
        LogHistogramValue(original_source,
                          one_click_signin::HISTOGRAM_WITH_ADVANCED);
        LogHistogramValue(original_source,
                          one_click_signin::HISTOGRAM_ACCEPTED);
      } else {
        LogHistogramValue(source_, one_click_signin::HISTOGRAM_ACCEPTED);
        LogHistogramValue(source_, one_click_signin::HISTOGRAM_WITH_DEFAULTS);
      }

      // - If sign in was initiated from the NTP or the hotdog menu, sync with
      //   default settings.
      // - If sign in was initiated from the settings page for first time sync
      //   set up, show the advanced sync settings dialog.
      // - If sign in was initiated from the settings page due to a re-auth when
      //   sync was already setup, simply navigate back to the settings page.
      ProfileSyncService* sync_service =
          ProfileSyncServiceFactory::GetForProfile(profile);
      SigninErrorController* error_controller =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
              signin_error_controller();

      OneClickSigninSyncStarter::StartSyncMode start_mode =
          source_ == signin::SOURCE_SETTINGS ?
              (error_controller->HasError() &&
               sync_service && sync_service->HasSyncSetupCompleted()) ?
                  OneClickSigninSyncStarter::SHOW_SETTINGS_WITHOUT_CONFIGURE :
                  OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST :
              OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS;

      if (!HandleCrossAccountError(profile, session_index_, email_, password_,
              "", auto_accept_, source_, start_mode,
              CreateSyncStarterCallback())) {
        if (!do_not_start_sync_for_testing_) {
          StartSync(
              StartSyncArgs(profile, browser, auto_accept_,
                            session_index_, email_, password_, "",
                            contents,
                            untrusted_confirmation_required_, source_,
                            CreateSyncStarterCallback()),
              start_mode);
        }

        // If this explicit sign in is not from settings page/webstore, show
        // the NTP/Apps page after sign in completes. In the case of the
        // settings page, it will get auto-closed after sync setup. In the case
        // of webstore, it will redirect back to webstore.
        RedirectToNtpOrAppsPageIfNecessary(web_contents(), source_);
      }

      // Observe the sync service if the Webstore tab or the settings tab
      // requested a gaia sign in, so that when sign in and sync setup are
      // successful, we can redirect to the correct URL, or auto-close the gaia
      // sign in tab.
      if (original_source == signin::SOURCE_SETTINGS ||
          (original_source == signin::SOURCE_WEBSTORE_INSTALL &&
           source_ == signin::SOURCE_SETTINGS)) {
        // The observer deletes itself once it's done.
        new OneClickSigninSyncObserver(contents, original_continue_url_);
      }
      break;
    }
    case AUTO_ACCEPT_REJECTED_FOR_PROFILE:
      AddEmailToOneClickRejectedList(profile, email_);
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_REJECTED);
      break;
    default:
      NOTREACHED() << "Invalid auto_accept=" << auto_accept_;
      break;
  }

  CleanTransientState();
}

OneClickSigninSyncStarter::Callback
    OneClickSigninHelper::CreateSyncStarterCallback() {
  // The callback will only be invoked if this object is still alive when sync
  // setup is completed. This is correct because this object is only deleted
  // when the web contents that potentially shows a blank page is deleted.
  return base::Bind(&OneClickSigninHelper::SyncSetupCompletedCallback,
                    weak_pointer_factory_.GetWeakPtr());
}

void OneClickSigninHelper::SyncSetupCompletedCallback(
    OneClickSigninSyncStarter::SyncSetupResult result) {
  if (result == OneClickSigninSyncStarter::SYNC_SETUP_FAILURE &&
      web_contents()) {
    GURL current_url = web_contents()->GetVisibleURL();

    // If the web contents is showing a blank page and not about to be closed,
    // redirect to the NTP or apps page.
    if (signin::IsContinueUrlForWebBasedSigninFlow(current_url) &&
        !signin::IsAutoCloseEnabledInURL(original_continue_url_)) {
      RedirectToNtpOrAppsPage(
          web_contents(),
          signin::GetSourceForPromoURL(original_continue_url_));
    }
  }
}
