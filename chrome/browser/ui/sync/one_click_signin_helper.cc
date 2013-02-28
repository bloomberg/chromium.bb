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
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/supports_user_data.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
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
#include "chrome/browser/ui/sync/one_click_signin_infobar_delegate.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/sync/signin_histogram.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/one_click_signin_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/password_form.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(OneClickSigninHelper);

namespace {

// Set to true if this chrome instance is in the blue-button-on-white-bar
// experimental group.
bool use_blue_on_white = false;

// Add a specific email to the list of emails rejected for one-click
// sign-in, for this profile.
void AddEmailToOneClickRejectedList(Profile* profile,
                                    const std::string& email) {
  PrefService* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(new base::StringValue(email));
}

// Arguments used with StartSync function.  base::Bind() cannot support too
// many args for performance reasons, so they are packaged up into a struct.
struct StartSyncArgs {
  StartSyncArgs(Profile* profile,
                Browser* browser,
                OneClickSigninHelper::AutoAccept auto_accept,
                const std::string& session_index,
                const std::string& email,
                const std::string& password,
                bool force_same_tab_navigation)
      : profile(profile),
        browser(browser),
        auto_accept(auto_accept),
        session_index(session_index),
        email(email),
        password(password),
        force_same_tab_navigation(force_same_tab_navigation) {
  }

  Profile* profile;
  Browser* browser;
  OneClickSigninHelper::AutoAccept auto_accept;
  std::string session_index;
  std::string email;
  std::string password;
  bool force_same_tab_navigation;
};

// Start syncing with the given user information.
void StartSync(const StartSyncArgs& args,
               OneClickSigninSyncStarter::StartSyncMode start_mode) {
  if (start_mode == OneClickSigninSyncStarter::UNDO_SYNC) {
    UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse",
                              one_click_signin::HISTOGRAM_UNDO,
                              one_click_signin::HISTOGRAM_MAX);
    return;
  }
  // The starter deletes itself once its done.
  new OneClickSigninSyncStarter(args.profile, args.browser, args.session_index,
                                args.email, args.password, start_mode,
                                args.force_same_tab_navigation);

  int action = one_click_signin::HISTOGRAM_MAX;
  switch (args.auto_accept) {
    case OneClickSigninHelper::AUTO_ACCEPT_EXPLICIT:
      action =
          start_mode == OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS ?
              one_click_signin::HISTOGRAM_AUTO_WITH_DEFAULTS :
              one_click_signin::HISTOGRAM_AUTO_WITH_ADVANCED;
      break;
    case OneClickSigninHelper::AUTO_ACCEPT_ACCEPTED:
      action = one_click_signin::HISTOGRAM_AUTO_WITH_DEFAULTS;
      break;
    case OneClickSigninHelper::AUTO_ACCEPT_NONE:
      action =
          start_mode == OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS ?
              one_click_signin::HISTOGRAM_WITH_DEFAULTS :
              one_click_signin::HISTOGRAM_WITH_ADVANCED;
      break;
    case OneClickSigninHelper::AUTO_ACCEPT_CONFIGURE:
      DCHECK(start_mode == OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
      action = one_click_signin::HISTOGRAM_AUTO_WITH_ADVANCED;
      break;
    default:
      NOTREACHED() << "Invalid auto_accept: " << args.auto_accept;
      break;
  }

  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse", action,
                            one_click_signin::HISTOGRAM_MAX);
}

void StartExplicitSync(const StartSyncArgs& args,
                       content::WebContents* contents,
                       OneClickSigninSyncStarter::StartSyncMode start_mode,
                       int button) {
  if (button == IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_OK_BUTTON) {
    contents->GetController().LoadURL(
        GURL(chrome::kChromeUINewTabURL), content::Referrer(),
        content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
    chrome::ShowSettingsSubPage(args.browser,
                                std::string(chrome::kSearchUsersSubPage));
  } else {
    StartSync(args, start_mode);
  }
}

void ClearPendingEmailOnIOThread(content::ResourceContext* context) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  DCHECK(io_data);
  io_data->set_reverse_autologin_pending_email(std::string());
}

// Determines the source of the sign in and the continue URL.  Its either one
// of the known sign in access point (first run, NTP, menu, settings) or its
// an implicit sign in via another Google property.  In the former case,
// "service" is also checked to make sure its "chromiumsync".
SyncPromoUI::Source GetSigninSource(const GURL& url, GURL* continue_url) {
  std::string value;
  net::GetValueForKeyInQuery(url, "service", &value);
  bool possibly_an_explicit_signin = value == "chromiumsync";

  // Find the final continue URL for this sign in.  In some cases, Gaia can
  // continue to itself, with the original continue URL buried under a couple
  // of layers of indirection.  Peel those layers away.
  GURL local_continue_url = url;
  do {
    local_continue_url =
        SyncPromoUI::GetNextPageURLForSyncPromoURL(local_continue_url);
  } while (gaia::IsGaiaSignonRealm(local_continue_url.GetOrigin()));

  if (continue_url && local_continue_url.is_valid()) {
    DCHECK(!continue_url->is_valid() || *continue_url == local_continue_url);
    *continue_url = local_continue_url;
  }

  return possibly_an_explicit_signin ?
      SyncPromoUI::GetSourceForSyncPromoURL(local_continue_url) :
      SyncPromoUI::SOURCE_UNKNOWN;
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

// Constants for the modal dialog / bubble sign in to chrome confirmation
// experiment
const char kSignInToChromeDialogFieldTrialName[] = "SignInToChromeConfirmation";
const char kSignInConfirmBubbleGroupName[] = "Bubble";

class ConfirmEmailDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  // Callback indicating action performed by the user.  The argument to the
  // callback is the ID of the button pressed by the user.
  typedef base::Callback<void(int)> Callback;

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

  // TabModalConfirmDialogDelegate implementation:
  virtual string16 GetTitle() OVERRIDE {
    return l10n_util::GetStringUTF16(
        IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_TITLE);
  }

  virtual string16 GetMessage() OVERRIDE {
    return l10n_util::GetStringFUTF16(
        IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_MESSAGE,
        UTF8ToUTF16(last_email_),
        UTF8ToUTF16(email_));
  }

  virtual string16 GetAcceptButtonTitle() OVERRIDE {
    return l10n_util::GetStringUTF16(
        IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_OK_BUTTON);
  }

  virtual string16 GetCancelButtonTitle() OVERRIDE {
    return l10n_util::GetStringUTF16(
        IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_CANCEL_BUTTON);
  }

  virtual void OnAccepted() OVERRIDE {
    base::ResetAndReturn(&callback_).Run(
        IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_OK_BUTTON);
  }

  virtual void OnCanceled() OVERRIDE {
    base::ResetAndReturn(&callback_).Run(
        IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_CANCEL_BUTTON);
  }

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

}  // namespace

// The infobar asking the user if they want to use one-click sign in.
// TODO(rogerta): once we move to a web-based sign in flow, we can get rid
// of this infobar.
class OneClickInfoBarDelegateImpl : public OneClickSigninInfoBarDelegate {
 public:
  // Creates a one click signin delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const std::string& session_index,
                     const std::string& email,
                     const std::string& password);

 private:
  OneClickInfoBarDelegateImpl(InfoBarService* owner,
                               const std::string& session_index,
                               const std::string& email,
                               const std::string& password);
  virtual ~OneClickInfoBarDelegateImpl();

  // InfoBarDelegate overrides.
  virtual InfoBarAutomationType GetInfoBarAutomationType() const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;

  // ConfirmInfoBarDelegate overrides.
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // OneClickSigninInfoBarDelegate overrides.
  virtual void GetAlternateColors(AlternateColors* alt_colors) OVERRIDE;

  // Record the specified action in the histogram for one-click sign in.
  void RecordHistogramAction(int action);

  // Information about the account that has just logged in.
  std::string session_index_;
  std::string email_;
  std::string password_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(OneClickInfoBarDelegateImpl);
};

// static
void OneClickInfoBarDelegateImpl::Create(InfoBarService* infobar_service,
                                         const std::string& session_index,
                                         const std::string& email,
                                         const std::string& password) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new OneClickInfoBarDelegateImpl(infobar_service, session_index, email,
                                      password)));
}

OneClickInfoBarDelegateImpl::OneClickInfoBarDelegateImpl(
    InfoBarService* owner,
    const std::string& session_index,
    const std::string& email,
    const std::string& password)
    : OneClickSigninInfoBarDelegate(owner),
      session_index_(session_index),
      email_(email),
      password_(password),
      button_pressed_(false) {
  RecordHistogramAction(one_click_signin::HISTOGRAM_SHOWN);
}

OneClickInfoBarDelegateImpl::~OneClickInfoBarDelegateImpl() {
  if (!button_pressed_)
    RecordHistogramAction(one_click_signin::HISTOGRAM_IGNORED);
}

InfoBarDelegate::InfoBarAutomationType
    OneClickInfoBarDelegateImpl::GetInfoBarAutomationType() const {
  return ONE_CLICK_LOGIN_INFOBAR;
}

void OneClickInfoBarDelegateImpl::InfoBarDismissed() {
  RecordHistogramAction(one_click_signin::HISTOGRAM_DISMISSED);
  button_pressed_ = true;
}

gfx::Image* OneClickInfoBarDelegateImpl::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_SYNC);
}

InfoBarDelegate::Type OneClickInfoBarDelegateImpl::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 OneClickInfoBarDelegateImpl::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_INFOBAR_MESSAGE);
}

string16 OneClickInfoBarDelegateImpl::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_ONE_CLICK_SIGNIN_INFOBAR_OK_BUTTON
                            : IDS_ONE_CLICK_SIGNIN_INFOBAR_CANCEL_BUTTON);
}

bool OneClickInfoBarDelegateImpl::Accept() {
  content::WebContents* web_contents = owner()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());

  // User has accepted one-click sign-in for this account. Never ask again for
  // this profile.
  SigninManager::DisableOneClickSignIn(profile);
  RecordHistogramAction(one_click_signin::HISTOGRAM_ACCEPTED);
  chrome::FindBrowserWithWebContents(web_contents)->window()->
      ShowOneClickSigninBubble(
          BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE,
          base::Bind(&StartSync,
                     StartSyncArgs(profile, browser,
                                   OneClickSigninHelper::AUTO_ACCEPT_NONE,
                                   session_index_, email_, password_,
                                   false /* force_same_tab_navigation */)));
  button_pressed_ = true;
  return true;
}

bool OneClickInfoBarDelegateImpl::Cancel() {
  AddEmailToOneClickRejectedList(Profile::FromBrowserContext(
      owner()->GetWebContents()->GetBrowserContext()), email_);
  RecordHistogramAction(one_click_signin::HISTOGRAM_REJECTED);
  button_pressed_ = true;
  return true;
}

string16 OneClickInfoBarDelegateImpl::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool OneClickInfoBarDelegateImpl::LinkClicked(
    WindowOpenDisposition disposition) {
  RecordHistogramAction(one_click_signin::HISTOGRAM_LEARN_MORE);
  content::OpenURLParams params(
      GURL(chrome::kChromeSyncLearnMoreURL), content::Referrer(), disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

void OneClickInfoBarDelegateImpl::GetAlternateColors(
    AlternateColors* alt_colors) {
  if (use_blue_on_white) {
    alt_colors->enabled = true;
    alt_colors->infobar_bottom_color = SK_ColorWHITE;
    alt_colors->infobar_top_color = SK_ColorWHITE;
    alt_colors->button_text_color = SK_ColorWHITE;
    alt_colors->button_background_color = SkColorSetRGB(71, 135, 237);
    alt_colors->button_border_color = SkColorSetRGB(48, 121, 237);
    return;
  }

  return OneClickSigninInfoBarDelegate::GetAlternateColors(alt_colors);
}

void OneClickInfoBarDelegateImpl::RecordHistogramAction(int action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse", action,
                            one_click_signin::HISTOGRAM_MAX);
}

OneClickSigninHelper::OneClickSigninHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      auto_accept_(AUTO_ACCEPT_NONE),
      source_(SyncPromoUI::SOURCE_UNKNOWN) {
}

OneClickSigninHelper::~OneClickSigninHelper() {
  content::WebContents* contents = web_contents();
  if (contents) {
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile);
    if (sync_service && sync_service->HasObserver(this))
      sync_service->RemoveObserver(this);
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

  if (!SigninManager::AreSigninCookiesAllowed(profile))
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

    if (!SyncPromoUI::UseWebBasedSigninFlow()) {
      // If we're about to show a one-click infobar but the user has started
      // a concurrent signin flow (perhaps via the promo), we may not have yet
      // established an authenticated username but we still shouldn't move
      // forward with two simultaneous signin processes.  This is a bit
      // contentious as the one-click flow is a much smoother flow from the user
      // perspective, but it's much more difficult to hijack the other flow from
      // here as it is to bail.
      ProfileSyncService* service =
          ProfileSyncServiceFactory::GetForProfile(profile);
      if (!service)
        return false;

      if (service->FirstSetupInProgress())
        return false;
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

  if (!SyncPromoUI::UseWebBasedSigninFlow())
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

  if (!SigninManager::AreSigninCookiesAllowed(io_data->GetCookieSettings()))
    return DONT_OFFER;

  // The checks below depend on chrome already knowing what account the user
  // signed in with.  This happens only after receiving the response containing
  // the Google-Accounts-SignIn header.  Until then, if there is even a chance
  // that we want to connect the profile, chrome needs to tell Gaia that
  // it should offer the interstitial.  Therefore missing one click data on
  // the request means can offer is true.
  const std::string& pending_email = io_data->reverse_autologin_pending_email();
  if (!pending_email.empty()) {
    if (!SigninManager::IsAllowedUsername(pending_email,
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
void OneClickSigninHelper::InitializeFieldTrial() {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("OneClickSignIn", 100,
                                                 "Standard", 2013, 9, 1, NULL));

  // For dev and beta, we'll give half the people the new experience.  For
  // stable, only 1%.  These numbers are overridable on the server.
  const bool kIsStableChannel =
      chrome::VersionInfo::GetChannel() == chrome::VersionInfo::CHANNEL_STABLE;
  const int kBlueOnWhiteGroup = trial->AppendGroup("BlueOnWhite",
                                                   kIsStableChannel ? 1 : 50);
  use_blue_on_white = trial->group() == kBlueOnWhiteGroup;
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

  if (!SyncPromoUI::UseWebBasedSigninFlow() &&
      google_accounts_signin_value.empty()) {
    return;
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
  if (SyncPromoUI::UseWebBasedSigninFlow() && !email.empty())
    io_data->set_reverse_autologin_pending_email(email);

  if (!email.empty() || !session_index.empty()) {
    VLOG(1) << "OneClickSigninHelper::ShowInfoBarIfPossible:"
            << " email=" << email
            << " sessionindex=" << session_index;
  }

  // Parse Google-Chrome-SignIn.
  AutoAccept auto_accept = AUTO_ACCEPT_NONE;
  SyncPromoUI::Source source = SyncPromoUI::SOURCE_UNKNOWN;
  GURL continue_url;
  if (SyncPromoUI::UseWebBasedSigninFlow()) {
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

    // If this is an explicit sign in (i.e., first run, NTP, menu,settings)
    // then force the auto accept type to explicit.
    source = GetSigninSource(request->original_url(), &continue_url);
    if (source != SyncPromoUI::SOURCE_UNKNOWN)
      auto_accept = AUTO_ACCEPT_EXPLICIT;
  }

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
void OneClickSigninHelper::ShowInfoBarUIThread(
    const std::string& session_index,
    const std::string& email,
    AutoAccept auto_accept,
    SyncPromoUI::Source source,
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

  // Save the email in the one-click signin manager.  The manager may
  // not exist if the contents is incognito or if the profile is already
  // connected to a Google account.
  if (!session_index.empty())
    helper->session_index_ = session_index;

  if (!email.empty())
    helper->email_ = email;

  if (auto_accept != AUTO_ACCEPT_NONE) {
    helper->auto_accept_ = auto_accept;
    helper->source_ = source;
  }

  CanOfferFor can_offer_for =
      (auto_accept != AUTO_ACCEPT_EXPLICIT &&
          helper->auto_accept_ != AUTO_ACCEPT_EXPLICIT) ?
          CAN_OFFER_FOR_INTERSTITAL_ONLY : CAN_OFFER_FOR_ALL;
  std::string error_message;

  if (!web_contents || !CanOffer(web_contents, can_offer_for, email,
                                 &error_message)) {
    VLOG(1) << "OneClickSigninHelper::ShowInfoBarUIThread: not offering";
    if (helper && helper->error_message_.empty() && !error_message.empty())
      helper->error_message_ = error_message;

    return;
  }

  if (continue_url.is_valid())
    helper->continue_url_ = continue_url;
}

void OneClickSigninHelper::RedirectToNTP(bool show_bubble) {
  VLOG(1) << "OneClickSigninHelper::RedirectToNTP";

  // Redirect to NTP with sign in bubble visible.
  content::WebContents* contents = web_contents();
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  PrefService* pref_service = profile->GetPrefs();
  if (show_bubble) {
    pref_service->SetBoolean(prefs::kSyncPromoShowNTPBubble, true);
    pref_service->SetString(prefs::kSyncPromoErrorMessage, error_message_);
  }

  content::OpenURLParams params(
      GURL(chrome::kChromeUINewTabURL),
      content::Referrer(),
      CURRENT_TAB,
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false);
  contents->OpenURL(params);

  error_message_.clear();
}

void OneClickSigninHelper::RedirectToSignin() {
  VLOG(1) << "OneClickSigninHelper::RedirectToSignin";

  // Extract the existing sounce=X value.  Default to "2" if missing.
  SyncPromoUI::Source source =
      SyncPromoUI::GetSourceForSyncPromoURL(continue_url_);
  if (source == SyncPromoUI::SOURCE_UNKNOWN)
    source = SyncPromoUI::SOURCE_MENU;
  GURL page = SyncPromoUI::GetSyncPromoURL(GURL(), source, false);

  content::WebContents* contents = web_contents();
  contents->GetController().LoadURL(page,
                                    content::Referrer(),
                                    content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                    std::string());
}

void OneClickSigninHelper::CleanTransientState() {
  VLOG(1) << "OneClickSigninHelper::CleanTransientState";
  email_.clear();
  password_.clear();
  auto_accept_ = AUTO_ACCEPT_NONE;
  source_ = SyncPromoUI::SOURCE_UNKNOWN;
  continue_url_ = GURL();

  // Post to IO thread to clear pending email.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&ClearPendingEmailOnIOThread,
                 base::Unretained(profile->GetResourceContext())));
}

bool OneClickSigninHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(OneClickSigninHelper, message)
    IPC_MESSAGE_HANDLER(OneClickSigninHostMsg_FormSubmitted, OnFormSubmitted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool OneClickSigninHelper::OnFormSubmitted(const content::PasswordForm& form) {
  // |password_| used to be set in DidNavigateAnyFrame, this is too late because
  // it is not executed until the end of redirect chains and password may
  // get lost if one of the redirects requires context swap.

  // We only need to scrape the password for Gaia logins.
  if (form.origin.is_valid() &&
      gaia::IsGaiaSignonRealm(GURL(form.signon_realm))) {
    VLOG(1) << "OneClickSigninHelper::DidNavigateAnyFrame: got password";
    password_ = UTF16ToUTF8(form.password_value);
  }

  return true;
}

void OneClickSigninHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  // If the user left the sign in process, clear all members.
  // TODO(rogerta): might need to allow some youtube URLs.
  content::WebContents* contents = web_contents();
  const GURL url = contents->GetURL();
  VLOG(1) << "OneClickSigninHelper::DidStopLoading: url=" << url.spec();

  // If an error has already occured during the sign in flow, make sure to
  // display it to the user and abort the process.  Do this only for
  // explicit sign ins.
  if (!error_message_.empty() && auto_accept_ == AUTO_ACCEPT_EXPLICIT) {
    VLOG(1) << "OneClickSigninHelper::DidStopLoading: error=" << error_message_;
    RedirectToNTP(true);
    return;
  }

  // When Gaia finally redirects to the continue URL, Gaia will add some
  // extra query parameters.  So ignore the parameters when checking to see
  // if the user has continued.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  const bool continue_url_match_accept = (
      auto_accept_ == AUTO_ACCEPT_EXPLICIT &&
      continue_url_.is_valid() &&
      url.ReplaceComponents(replacements) ==
        continue_url_.ReplaceComponents(replacements));

  // If there is no valid email or password yet, there is nothing to do.
  if (email_.empty() || password_.empty()) {
    if (continue_url_match_accept)
      RedirectToSignin();
    std::string unused_value;
    if (net::GetValueForKeyInQuery(url, "ntp", &unused_value))
      RedirectToNTP(false);
    return;
  }

  // When the user uses the first-run, ntp, or hotdog menu to sign in, then have
  // the option of checking the the box "Let me choose what to sync".  When the
  // sign in process started, the source parameter in the continue URL may have
  // indicated one of the three options above.  However, once this box is
  // checked, the source parameter will indicate settings.  This will only be
  // communicated back to chrome when Gaia redirects to the continue URL, and
  // this is considered here a last minute change to the source.  See a little
  // further below for when this variable is set to true.
  bool force_same_tab_navigation = false;

  if (SyncPromoUI::UseWebBasedSigninFlow()) {
    if (IsValidGaiaSigninRedirectOrResponseURL(url))
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
      DCHECK(source_ != SyncPromoUI::SOURCE_UNKNOWN);
      if (!continue_url_match_accept) {
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
      SyncPromoUI::Source source =
          SyncPromoUI::GetSourceForSyncPromoURL(url);
      if (source != source_) {
        source_ = source;
        force_same_tab_navigation = source_ == SyncPromoUI::SOURCE_SETTINGS;
      }
    }
  }

  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());

  VLOG(1) << "OneClickSigninHelper::DidStopLoading: signin is go."
          << " auto_accept=" << auto_accept_
          << " source=" << source_;

  BrowserWindow::OneClickSigninBubbleType bubble_type;
  if (base::FieldTrialList::FindFullName(kSignInToChromeDialogFieldTrialName) ==
      kSignInConfirmBubbleGroupName)
    bubble_type = BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE;
  else
    bubble_type = BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_MODAL_DIALOG;

  switch (auto_accept_) {
    case AUTO_ACCEPT_NONE:
      if (SyncPromoUI::UseWebBasedSigninFlow()) {
        UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse",
                                  one_click_signin::HISTOGRAM_DISMISSED,
                                  one_click_signin::HISTOGRAM_MAX);
      } else {
        OneClickInfoBarDelegateImpl::Create(
            InfoBarService::FromWebContents(contents), session_index_, email_,
            password_);
      }
      break;
    case AUTO_ACCEPT_ACCEPTED:
      SigninManager::DisableOneClickSignIn(profile);
      browser->window()->ShowOneClickSigninBubble(
          bubble_type,
          base::Bind(&StartSync,
                     StartSyncArgs(profile, browser, auto_accept_,
                                   session_index_, email_, password_,
                                   false /* force_same_tab_navigation */)));
      break;
    case AUTO_ACCEPT_CONFIGURE:
      SigninManager::DisableOneClickSignIn(profile);
      StartSync(
          StartSyncArgs(profile, browser, auto_accept_, session_index_, email_,
                        password_, false /* force_same_tab_navigation */),
          OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
      break;
    case AUTO_ACCEPT_EXPLICIT: {
      OneClickSigninSyncStarter::StartSyncMode start_mode =
          source_ == SyncPromoUI::SOURCE_SETTINGS ?
              OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST :
              OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS;

      // If the new email address is different from the email address that
      // just signed in, show a confirmation dialog.
      std::string last_email =
          profile->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
      if (!last_email.empty() && last_email != email_) {
        ConfirmEmailDialogDelegate::AskForConfirmation(
            contents,
            last_email,
            email_,
            base::Bind(
                &StartExplicitSync,
                StartSyncArgs(profile, browser, auto_accept_,
                              session_index_, email_, password_,
                              force_same_tab_navigation),
                contents,
                start_mode));
      } else {
        StartExplicitSync(
            StartSyncArgs(profile, browser, auto_accept_, session_index_,
                          email_, password_, force_same_tab_navigation),
            contents,
            start_mode,
            IDS_ONE_CLICK_SIGNIN_CONFIRM_EMAIL_DIALOG_CANCEL_BUTTON);
      }

      if (source_ == SyncPromoUI::SOURCE_SETTINGS &&
          SyncPromoUI::GetSourceForSyncPromoURL(continue_url_) ==
          SyncPromoUI::SOURCE_WEBSTORE_INSTALL) {
        redirect_url_ = continue_url_;
        ProfileSyncService* sync_service =
          ProfileSyncServiceFactory::GetForProfile(profile);
        if (sync_service)
          sync_service->AddObserver(this);
      }

      // If this explicit sign in is not from settings page/webstore, show the
      // NTP after sign in completes.  In the case of the settings page, it will
      // get closed by SyncSetupHandler. In the case of webstore, it will
      // redirect back to webstore.
      if (source_ != SyncPromoUI::SOURCE_SETTINGS &&
          source_ != SyncPromoUI::SOURCE_WEBSTORE_INSTALL) {
        signin_tracker_.reset(new SigninTracker(profile, this));
        RedirectToNTP(true);
      }
      break;
    }
    case AUTO_ACCEPT_REJECTED_FOR_PROFILE:
      AddEmailToOneClickRejectedList(profile, email_);
      UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse",
                                one_click_signin::HISTOGRAM_REJECTED,
                                one_click_signin::HISTOGRAM_MAX);
      break;
    default:
      NOTREACHED() << "Invalid auto_accept=" << auto_accept_;
      break;
  }

  CleanTransientState();
}

void OneClickSigninHelper::GaiaCredentialsValid() {
}

void OneClickSigninHelper::OnStateChanged() {
  // No redirect url after sync setup is set, thus no need to watch for sync
  // state changes.
  if (redirect_url_.is_empty())
    return;

  content::WebContents* contents = web_contents();
  Profile* profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  // Sync setup not completed yet.
  if (sync_service->FirstSetupInProgress())
    return;

  if (sync_service->sync_initialized()) {
    contents->GetController().LoadURL(redirect_url_,
                                      content::Referrer(),
                                      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                      std::string());
  }

  // Clear the redirect URL.
  redirect_url_ = GURL();
  sync_service->RemoveObserver(this);
}

void OneClickSigninHelper::SigninFailed(const GoogleServiceAuthError& error) {
  if (error_message_.empty() && !error.error_message().empty())
      error_message_ = error.error_message();

  if (error_message_.empty()) {
    switch (error.state()) {
      case GoogleServiceAuthError::NONE:
        error_message_.clear();
        break;
      case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
        error_message_ = l10n_util::GetStringUTF8(IDS_SYNC_UNRECOVERABLE_ERROR);
        break;
      default:
        error_message_ = l10n_util::GetStringUTF8(IDS_SYNC_ERROR_SIGNING_IN);
        break;
    }
  }

  RedirectToNTP(true);
  signin_tracker_.reset();
}

void OneClickSigninHelper::SigninSuccess() {
  signin_tracker_.reset();
}
