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
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/chrome_signin_manager_delegate.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_delegate.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/sync/signin_histogram.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/password_form.h"
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
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"


namespace {

// StartSyncArgs --------------------------------------------------------------

// Arguments used with StartSync function.  base::Bind() cannot support too
// many args for performance reasons, so they are packaged up into a struct.
struct StartSyncArgs {
  StartSyncArgs(Profile* profile,
                Browser* browser,
                OneClickSigninHelper::AutoAccept auto_accept,
                const std::string& session_index,
                const std::string& email,
                const std::string& password,
                content::WebContents* web_contents,
                bool untrusted_confirmation_required,
                signin::Source source,
                OneClickSigninSyncStarter::Callback callback);

  Profile* profile;
  Browser* browser;
  OneClickSigninHelper::AutoAccept auto_accept;
  std::string session_index;
  std::string email;
  std::string password;

  // Web contents in which the sync setup page should be displayed,
  // if necessary. Can be NULL.
  content::WebContents* web_contents;

  OneClickSigninSyncStarter::ConfirmationRequired confirmation_required;
  signin::Source source;
  OneClickSigninSyncStarter::Callback callback;
};

StartSyncArgs::StartSyncArgs(Profile* profile,
                             Browser* browser,
                             OneClickSigninHelper::AutoAccept auto_accept,
                             const std::string& session_index,
                             const std::string& email,
                             const std::string& password,
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
      web_contents(web_contents),
      source(source),
      callback(callback) {
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
  virtual string16 GetTitle() OVERRIDE;
  virtual string16 GetMessage() OVERRIDE;
  virtual string16 GetAcceptButtonTitle() OVERRIDE;
  virtual string16 GetCancelButtonTitle() OVERRIDE;
  virtual void OnAccepted() OVERRIDE;
  virtual void OnCanceled() OVERRIDE;
  virtual void OnClosed() OVERRIDE;

  std::string last_email_;
  std::string email_;
  Callback callback_;

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
    callback_(callback) {
}

ConfirmEmailDialogDelegate::~ConfirmEmailDialogDelegate() {
}

string16 ConfirmEmailDialogDelegate::GetTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_TITLE);
}

string16 ConfirmEmailDialogDelegate::GetMessage() {
  return l10n_util::GetStringFUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_MESSAGE,
      UTF8ToUTF16(last_email_), UTF8ToUTF16(email_));
}

string16 ConfirmEmailDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_OK_BUTTON);
}

string16 ConfirmEmailDialogDelegate::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_CANCEL_BUTTON);
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


// Helpers --------------------------------------------------------------------

// Add a specific email to the list of emails rejected for one-click
// sign-in, for this profile.
void AddEmailToOneClickRejectedList(Profile* profile,
                                    const std::string& email) {
  ListPrefUpdate updater(profile->GetPrefs(),
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(new base::StringValue(email));
}

void LogHistogramValue(signin::Source source, int action) {
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
    default:
      // This switch statement needs to be updated when the enum Source changes.
      COMPILE_ASSERT(signin::SOURCE_UNKNOWN == 9,
                     kSourceEnumHasChangedButNotThisSwitchStatement);
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            one_click_signin::HISTOGRAM_MAX);
}

void LogOneClickHistogramValue(int action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.OneClickActions", action,
                            one_click_signin::HISTOGRAM_MAX);
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            one_click_signin::HISTOGRAM_MAX);
}

void RedirectToNtpOrAppsPage(content::WebContents* contents,
                             signin::Source source) {
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

// If the |source| is not settings page/webstore, redirects to
// the NTP/Apps page.
void RedirectToNtpOrAppsPageIfNecessary(content::WebContents* contents,
                                        signin::Source source) {
  if (source != signin::SOURCE_SETTINGS &&
      source != signin::SOURCE_WEBSTORE_INSTALL) {
    RedirectToNtpOrAppsPage(contents, source);
  }
}

// Start syncing with the given user information.
void StartSync(const StartSyncArgs& args,
               OneClickSigninSyncStarter::StartSyncMode start_mode) {
  if (start_mode == OneClickSigninSyncStarter::UNDO_SYNC) {
    LogOneClickHistogramValue(one_click_signin::HISTOGRAM_UNDO);
    return;
  }

  // The starter deletes itself once its done.
  new OneClickSigninSyncStarter(args.profile, args.browser, args.session_index,
                                args.email, args.password, start_mode,
                                args.web_contents,
                                args.confirmation_required,
                                args.source,
                                args.callback);

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

void StartExplicitSync(const StartSyncArgs& args,
                       content::WebContents* contents,
                       OneClickSigninSyncStarter::StartSyncMode start_mode,
                       ConfirmEmailDialogDelegate::Action action) {
  if (action == ConfirmEmailDialogDelegate::START_SYNC) {
    StartSync(args, start_mode);
    RedirectToNtpOrAppsPageIfNecessary(contents, args.source);
  } else {
    if (signin::IsContinueUrlForWebBasedSigninFlow(contents->GetVisibleURL()))
      RedirectToNtpOrAppsPage(contents, args.source);
    if (action == ConfirmEmailDialogDelegate::CREATE_NEW_USER) {
      chrome::ShowSettingsSubPage(args.browser,
                                  std::string(chrome::kSearchUsersSubPage));
    }
  }
}

void ClearPendingEmailOnIOThread(content::ResourceContext* context) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  DCHECK(io_data);
  io_data->set_reverse_autologin_pending_email(std::string());
}

// Determines the source of the sign in and the continue URL.  Its either one
// of the known sign in access point (first run, NTP, Apps page, menu, settings)
// or its an implicit sign in via another Google property.  In the former case,
// "service" is also checked to make sure its "chromiumsync".
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
      GURL(GaiaUrls::GetInstance()->service_login_url()).ReplaceComponents(
          replacements);

  return (url.ReplaceComponents(replacements) == clean_login_url &&
          source != signin::SOURCE_UNKNOWN) ||
      (IsValidGaiaSigninRedirectOrResponseURL(url) &&
       url.spec().find("ChromeLoginPrompt") != std::string::npos &&
       !email.empty());
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
  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE;
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

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
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  // Return early if this is not top-level navigation.
  if (!is_main_frame)
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

void CurrentHistoryCleaner::WebContentsDestroyed(
    content::WebContents* contents) {
  delete this;  // Failure.
}

void CloseTab(content::WebContents* tab) {
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
  }
}

}  // namespace


// OneClickSigninHelper -------------------------------------------------------

DEFINE_WEB_CONTENTS_USER_DATA_KEY(OneClickSigninHelper);

// static
const int OneClickSigninHelper::kMaxNavigationsSince = 10;

OneClickSigninHelper::OneClickSigninHelper(content::WebContents* web_contents,
                                           PasswordManager* password_manager)
    : content::WebContentsObserver(web_contents),
      showing_signin_(false),
      auto_accept_(AUTO_ACCEPT_NONE),
      source_(signin::SOURCE_UNKNOWN),
      switched_to_advanced_(false),
      untrusted_navigations_since_signin_visit_(0),
      untrusted_confirmation_required_(false),
      do_not_clear_pending_email_(false),
      weak_pointer_factory_(this) {
  // May be NULL during testing.
  if (password_manager) {
    password_manager->AddSubmissionCallback(
        base::Bind(&OneClickSigninHelper::PasswordSubmitted,
                   weak_pointer_factory_.GetWeakPtr()));
  }
}

OneClickSigninHelper::~OneClickSigninHelper() {
  // WebContentsDestroyed() should always be called before the object is
  // deleted.
  DCHECK(!web_contents());
}

// static
void OneClickSigninHelper::CreateForWebContentsWithPasswordManager(
    content::WebContents* contents,
    PasswordManager* password_manager) {
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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

  if (!ChromeSigninManagerDelegate::ProfileAllowsSigninCookies(profile))
    return false;

  if (!email.empty()) {
    if (!manager)
      return false;

    // If the signin manager already has an authenticated name, then this is a
    // re-auth scenario.  Make sure the email just signed in corresponds to the
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
                                      UTF8ToUTF16(current_email)));
      }
      return false;
    }

    // Make sure this username is not prohibited by policy.
    if (!manager->IsAllowedUsername(email)) {
      if (error_message) {
        error_message->assign(
            l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED));
      }
      return false;
    }

    // If some profile, not just the current one, is already connected to this
    // account, don't show the infobar.
    if (g_browser_process && !same_email) {
      ProfileManager* manager = g_browser_process->profile_manager();
      if (manager) {
        string16 email16 = UTF8ToUTF16(email);
        ProfileInfoCache& cache = manager->GetProfileInfoCache();

        for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
          if (email16 == cache.GetUserNameOfProfileAtIndex(i)) {
            if (error_message) {
              error_message->assign(
                  l10n_util::GetStringUTF8(IDS_SYNC_USER_NAME_IN_USE_ERROR));
            }
            return false;
          }
        }
      }
    }

    // If email was already rejected by this profile for one-click sign-in.
    if (can_offer_for == CAN_OFFER_FOR_INTERSTITAL_ONLY) {
      const ListValue* rejected_emails = profile->GetPrefs()->GetList(
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
  return CanOfferOnIOThreadImpl(request->url(), request->referrer(),
                                request, io_data);
}

// static
OneClickSigninHelper::Offer OneClickSigninHelper::CanOfferOnIOThreadImpl(
    const GURL& url,
    const std::string& referrer,
    base::SupportsUserData* request,
    ProfileIOData* io_data) {
  if (!gaia::IsGaiaSignonRealm(url.GetOrigin()))
    return IGNORE_REQUEST;

  if (!io_data)
    return DONT_OFFER;

  // Check for incognito before other parts of the io_data, since those
  // members may not be initalized.
  if (io_data->is_incognito())
    return DONT_OFFER;

  if (!SigninManager::IsSigninAllowedOnIOThread(io_data))
    return DONT_OFFER;

  if (!io_data->reverse_autologin_enabled()->GetValue())
    return DONT_OFFER;

  if (!io_data->google_services_username()->GetValue().empty())
    return DONT_OFFER;

  if (!ChromeSigninManagerDelegate::SettingsAllowSigninCookies(
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
            UTF8ToUTF16(pending_email)) > 0) {
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

  if (!gaia::IsGaiaSignonRealm(request->original_url().GetOrigin()))
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
      TrimString(value, "\"", &email);
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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
  SigninManager* manager = profile ?
      SigninManagerFactory::GetForProfile(profile) : NULL;
  helper->untrusted_confirmation_required_ |=
      (manager && !manager->IsSigninProcess(child_id));

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

void OneClickSigninHelper::ShowSigninErrorBubble(Browser* browser,
                                                 const std::string& error) {
  DCHECK(!error.empty());

  browser->window()->ShowOneClickSigninBubble(
      BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
      string16(), /* no SAML email */
      UTF8ToUTF16(error),
      // This callback is never invoked.
      // TODO(rogerta): Separate out the bubble API so we don't have to pass
      // ignored |email| and |callback| params.
      BrowserWindow::StartSyncCallback());
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
    const content::PasswordForm& form) {
  // We only need to scrape the password for Gaia logins.
  if (gaia::IsGaiaSignonRealm(GURL(form.signon_realm))) {
    VLOG(1) << "OneClickSigninHelper::DidNavigateAnyFrame: got password";
    password_ = UTF16ToUTF8(form.password_value);
  }
}

void OneClickSigninHelper::SetDoNotClearPendingEmailForTesting() {
  do_not_clear_pending_email_ = true;
}

void OneClickSigninHelper::NavigateToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  VLOG(1) << "OneClickSigninHelper::NavigateToPendingEntry: url=" << url.spec();
  // If the tab navigates to a new page, and this page is not a valid Gaia
  // sign in redirect or reponse, or the expected continue URL, make sure to
  // clear the internal state.  This is needed to detect navigations in the
  // middle of the sign in process that may redirect back to the sign in
  // process (see crbug.com/181163 for details).
  const GURL continue_url = signin::GetNextPageURLForPromoURL(
      signin::GetPromoURL(signin::SOURCE_START_PAGE, false));
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
    SigninManager* manager = profile ?
        SigninManagerFactory::GetForProfile(profile) : NULL;
    int process_id = web_contents()->GetRenderProcessHost()->GetID();
    if (manager && manager->IsSigninProcess(process_id))
      manager->ClearSigninProcess();

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
  // if the user has continued.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  const bool continue_url_match = (
      continue_url_.is_valid() &&
      url.ReplaceComponents(replacements) ==
        continue_url_.ReplaceComponents(replacements));

  if (continue_url_match)
    RemoveSigninRedirectURLHistoryItem(contents);

  // If there is no valid email yet, there is nothing to do.  As of M26, the
  // password is allowed to be empty, since its no longer required to setup
  // sync.
  if (email_.empty()) {
    VLOG(1) << "OneClickSigninHelper::DidStopLoading: nothing to do";
    if (continue_url_match && auto_accept_ == AUTO_ACCEPT_EXPLICIT)
      RedirectToSignin();
    std::string unused_value;
    if (net::GetValueForKeyInQuery(url, "ntp", &unused_value)) {
      signin::SetUserSkippedPromo(profile);
      RedirectToNtpOrAppsPage(web_contents(), source_);
    }

    if (!continue_url_match && !IsValidGaiaSigninRedirectOrResponseURL(url) &&
        ++untrusted_navigations_since_signin_visit_ > kMaxNavigationsSince) {
      CleanTransientState();
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
      SigninManager::DisableOneClickSignIn(profile);
      // Start syncing with the default settings - prompt the user to sign in
      // first.
      StartSync(
          StartSyncArgs(profile, browser, auto_accept_,
                        session_index_, email_, password_,
                        NULL /* don't force to show sync setup in same tab */,
                        true /* confirmation_required */, source_,
                        CreateSyncStarterCallback()),
          OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
      break;
    case AUTO_ACCEPT_CONFIGURE:
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_ACCEPTED);
      LogOneClickHistogramValue(one_click_signin::HISTOGRAM_WITH_ADVANCED);
      SigninManager::DisableOneClickSignIn(profile);
      // Display the extra confirmation (even in the SAML case) in case this
      // was an untrusted renderer.
      StartSync(
          StartSyncArgs(profile, browser, auto_accept_,
                        session_index_, email_, password_,
                        NULL  /* don't force to show sync setup in same tab */,
                        true /* confirmation_required */, source_,
                        CreateSyncStarterCallback()),
          OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
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
      // - If sign in was initiated from the settings page due to a re-auth,
      //   simply navigate back to the settings page.
      OneClickSigninSyncStarter::StartSyncMode start_mode =
          source_ == signin::SOURCE_SETTINGS ?
              SigninGlobalError::GetForProfile(profile)->HasMenuItem() ?
                  OneClickSigninSyncStarter::SHOW_SETTINGS_WITHOUT_CONFIGURE :
                  OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST :
              OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS;

      std::string last_email =
          profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);

      if (!last_email.empty() && !gaia::AreEmailsSame(last_email, email_)) {
        // If the new email address is different from the email address that
        // just signed in, show a confirmation dialog.

        // No need to display a second confirmation so pass false below.
        // TODO(atwilson): Move this into OneClickSigninSyncStarter.
        // The tab modal dialog always executes its callback before |contents|
        // is deleted.
        ConfirmEmailDialogDelegate::AskForConfirmation(
            contents,
            last_email,
            email_,
            base::Bind(
                &StartExplicitSync,
                StartSyncArgs(profile, browser, auto_accept_,
                              session_index_, email_, password_, contents,
                              false /* confirmation_required */, source_,
                              CreateSyncStarterCallback()),
                contents,
                start_mode));
      } else {
        StartSync(
            StartSyncArgs(profile, browser, auto_accept_,
                          session_index_, email_, password_, contents,
                          untrusted_confirmation_required_, source_,
                          CreateSyncStarterCallback()),
            start_mode);

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
        ProfileSyncService* sync_service =
            ProfileSyncServiceFactory::GetForProfile(profile);
        if (sync_service)
          sync_service->AddObserver(this);
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

// It is guaranteed that this method is called before the object is deleted.
void OneClickSigninHelper::WebContentsDestroyed(
    content::WebContents* contents) {
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (sync_service)
    sync_service->RemoveObserver(this);
}

void OneClickSigninHelper::OnStateChanged() {
  // We only add observer for ProfileSyncService when original_continue_url_ is
  // not empty.
  DCHECK(!original_continue_url_.is_empty());

  content::WebContents* contents = web_contents();
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // At this point, the sign in process is complete, and control has been handed
  // back to the sync engine. Close the gaia sign in tab if
  // |original_continue_url_| contains the |auto_close| parameter. Otherwise,
  // wait for sync setup to complete and then navigate to
  // |original_continue_url_|.
  if (signin::IsAutoCloseEnabledInURL(original_continue_url_)) {
    // Close the gaia sign in tab via a task to make sure we aren't in the
    // middle of any webui handler code.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CloseTab, base::Unretained(contents)));
  } else {
    // Sync setup not completed yet.
    if (sync_service->FirstSetupInProgress())
      return;

    if (sync_service->sync_initialized()) {
      contents->GetController().LoadURL(original_continue_url_,
                                        content::Referrer(),
                                        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
    }
  }

  // Clears |original_continue_url_| here instead of in CleanTransientState,
  // because it is used in OnStateChanged which occurs later.
  original_continue_url_ = GURL();
  sync_service->RemoveObserver(this);
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
