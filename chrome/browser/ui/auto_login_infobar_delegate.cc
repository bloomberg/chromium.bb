// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_infobar_delegate.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/infobars/core/infobar.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/ubertoken_fetcher.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/infobars/auto_login_infobar_delegate_android.h"
#endif


// AutoLoginRedirector --------------------------------------------------------

namespace {

// This class is created by the AutoLoginInfoBarDelegate when the user wishes to
// auto-login.  It holds context information needed while re-issuing service
// tokens using the OAuth2TokenService, gets the browser cookies with the
// TokenAuth API, and finally redirects the user to the correct page.
class AutoLoginRedirector : public UbertokenConsumer,
                            public content::WebContentsObserver {
 public:
  AutoLoginRedirector(content::WebContents* web_contents,
                      const std::string& args);
  virtual ~AutoLoginRedirector();

 private:
  // Overriden from UbertokenConsumer:
  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Implementation of content::WebContentsObserver
  virtual void WebContentsDestroyed() OVERRIDE;

  // Redirect tab to MergeSession URL, logging the user in and navigating
  // to the desired page.
  void RedirectToMergeSession(const std::string& token);

  const std::string args_;
  scoped_ptr<UbertokenFetcher> ubertoken_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginRedirector);
};

AutoLoginRedirector::AutoLoginRedirector(
    content::WebContents* web_contents,
    const std::string& args)
    : content::WebContentsObserver(web_contents),
      args_(args) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile);
  ubertoken_fetcher_.reset(new UbertokenFetcher(token_service,
                                                this,
                                                profile->GetRequestContext()));
  ubertoken_fetcher_->StartFetchingToken(
      signin_manager->GetAuthenticatedAccountId());
}

AutoLoginRedirector::~AutoLoginRedirector() {
}

void AutoLoginRedirector::WebContentsDestroyed() {
  // The WebContents that started this has been destroyed. The request must be
  // cancelled and this object must be deleted.
  ubertoken_fetcher_.reset();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::OnUbertokenSuccess(const std::string& token) {
  RedirectToMergeSession(token);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "AutoLoginRedirector: token request failed";
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::RedirectToMergeSession(const std::string& token) {
  // TODO(rogerta): what is the correct page transition?
  web_contents()->GetController().LoadURL(
      GaiaUrls::GetInstance()->merge_session_url().Resolve(
          "?source=chrome&uberauth=" + token + "&" + args_),
      content::Referrer(), content::PAGE_TRANSITION_AUTO_BOOKMARK,
      std::string());
}

}  // namespace


// AutoLoginInfoBarDelegate ---------------------------------------------------

// static
bool AutoLoginInfoBarDelegate::Create(content::WebContents* web_contents,
                                      const Params& params) {
  // If |web_contents| is hosted in a WebDialog, there may be no infobar
  // service.
  InfoBarService* infobar_service =
    InfoBarService::FromWebContents(web_contents);
  if (!infobar_service)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
#if defined(OS_ANDROID)
  typedef AutoLoginInfoBarDelegateAndroid Delegate;
#else
  typedef AutoLoginInfoBarDelegate Delegate;
#endif
  return !!infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new Delegate(params, profile))));
}

AutoLoginInfoBarDelegate::AutoLoginInfoBarDelegate(const Params& params,
                                                   Profile* profile)
    : ConfirmInfoBarDelegate(),
      params_(params),
      profile_(profile),
      button_pressed_(false) {
  RecordHistogramAction(SHOWN);

  // The AutoLogin infobar is shown in incognito mode on Android, so a
  // SigninManager isn't guaranteed to exist for |profile_|.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile_);
  if (signin_manager)
    signin_manager->AddObserver(this);
}

AutoLoginInfoBarDelegate::~AutoLoginInfoBarDelegate() {
  // The AutoLogin infobar is shown in incognito mode on Android, so a
  // SigninManager isn't guaranteed to exist for |profile_|.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile_);
  if (signin_manager)
    signin_manager->RemoveObserver(this);

  if (!button_pressed_)
    RecordHistogramAction(IGNORED);
}

void AutoLoginInfoBarDelegate::RecordHistogramAction(Actions action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Regular", action,
                            HISTOGRAM_BOUNDING_VALUE);
}

void AutoLoginInfoBarDelegate::InfoBarDismissed() {
  RecordHistogramAction(DISMISSED);
  button_pressed_ = true;
}

int AutoLoginInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_AUTOLOGIN;
}

infobars::InfoBarDelegate::Type AutoLoginInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

AutoLoginInfoBarDelegate*
    AutoLoginInfoBarDelegate::AsAutoLoginInfoBarDelegate() {
  return this;
}

base::string16 AutoLoginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_AUTOLOGIN_INFOBAR_MESSAGE,
                                    base::UTF8ToUTF16(params_.username));
}

base::string16 AutoLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOLOGIN_INFOBAR_OK_BUTTON : IDS_AUTOLOGIN_INFOBAR_CANCEL_BUTTON);
}

bool AutoLoginInfoBarDelegate::Accept() {
  // AutoLoginRedirector deletes itself.
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  new AutoLoginRedirector(web_contents, params_.header.args);
  RecordHistogramAction(ACCEPTED);
  button_pressed_ = true;
  return true;
}

bool AutoLoginInfoBarDelegate::Cancel() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  PrefService* pref_service = Profile::FromBrowserContext(
      web_contents->GetBrowserContext())->GetPrefs();
  pref_service->SetBoolean(prefs::kAutologinEnabled, false);
  RecordHistogramAction(REJECTED);
  button_pressed_ = true;
  return true;
}

void AutoLoginInfoBarDelegate::GoogleSignedOut(const std::string& username) {
  infobar()->RemoveSelf();
}
