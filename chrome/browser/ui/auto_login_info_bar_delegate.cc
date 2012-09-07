// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_info_bar_delegate.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/ubertoken_fetcher.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationController;
using content::NotificationSource;
using content::NotificationDetails;

namespace {

// Enum values used for UMA histograms.
enum {
  // The infobar was shown to the user.
  HISTOGRAM_SHOWN,

  // The user pressed the accept button to perform the suggested action.
  HISTOGRAM_ACCEPTED,

  // The user pressed the reject to turn off the feature.
  HISTOGRAM_REJECTED,

  // The user pressed the X button to dismiss the infobar this time.
  HISTOGRAM_DISMISSED,

  // The user completely ignored the infoar.  Either they navigated away, or
  // they used the page as is.
  HISTOGRAM_IGNORED,

  // The user clicked on the learn more link in the infobar.
  HISTOGRAM_LEARN_MORE,

  HISTOGRAM_MAX
};

// AutoLoginRedirector --------------------------------------------------------

// This class is created by the AutoLoginInfoBarDelegate when the user wishes to
// auto-login.  It holds context information needed while re-issuing service
// tokens using the TokenService, gets the browser cookies with the TokenAuth
// API, and finally redirects the user to the correct page.
class AutoLoginRedirector : public UbertokenConsumer,
                            public content::NotificationObserver {
 public:
  AutoLoginRedirector(NavigationController* navigation_controller,
                      const std::string& args);
  virtual ~AutoLoginRedirector();

 private:
  // Overriden from UbertokenConsumer:
  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE;
  virtual void OnUbertokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Implementation of content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Redirect tab to MergeSession URL, logging the user in and navigating
  // to the desired page.
  void RedirectToMergeSession(const std::string& token);

  NavigationController* navigation_controller_;
  const std::string args_;
  scoped_ptr<UbertokenFetcher> ubertoken_fetcher_;

  // For listening to NavigationController destruction.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginRedirector);
};

AutoLoginRedirector::AutoLoginRedirector(
    NavigationController* navigation_controller,
    const std::string& args)
    : navigation_controller_(navigation_controller),
      args_(args) {
  ubertoken_fetcher_.reset(new UbertokenFetcher(
      Profile::FromBrowserContext(navigation_controller_->GetBrowserContext()),
      this));
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<content::WebContents>(
                     navigation_controller_->GetWebContents()));
  ubertoken_fetcher_->StartFetchingToken();
}

AutoLoginRedirector::~AutoLoginRedirector() {
}

void AutoLoginRedirector::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  // The WebContents that started this has been destroyed. The request must be
  // cancelled and this object must be deleted.
  ubertoken_fetcher_.reset();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::OnUbertokenSuccess(const std::string& token) {
  RedirectToMergeSession(token);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "AutoLoginRedirector: token request failed";
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoLoginRedirector::RedirectToMergeSession(const std::string& token) {
  // TODO(rogerta): what is the correct page transition?
  navigation_controller_->LoadURL(
      GURL(GaiaUrls::GetInstance()->merge_session_url() +
          "?source=chrome&uberauth=" + token + "&" + args_),
      content::Referrer(), content::PAGE_TRANSITION_AUTO_BOOKMARK,
      std::string());
}

}  // namepsace


// AutoLoginInfoBarDelegate ---------------------------------------------------

// AutoLoginInfoBarDelegate::Params -------------------------------------------

AutoLoginInfoBarDelegate::Params::Params() {}
AutoLoginInfoBarDelegate::Params::~Params() {}

AutoLoginInfoBarDelegate::AutoLoginInfoBarDelegate(
    InfoBarTabHelper* owner,
    const Params& params)
    : ConfirmInfoBarDelegate(owner),
      params_(params),
      button_pressed_(false) {
  RecordHistogramAction(HISTOGRAM_SHOWN);
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(Profile::FromBrowserContext(
                     owner->GetWebContents()->GetBrowserContext())));
}

AutoLoginInfoBarDelegate::~AutoLoginInfoBarDelegate() {
  if (!button_pressed_)
    RecordHistogramAction(HISTOGRAM_IGNORED);
}

AutoLoginInfoBarDelegate*
    AutoLoginInfoBarDelegate::AsAutoLoginInfoBarDelegate() {
  return this;
}

void AutoLoginInfoBarDelegate::InfoBarDismissed() {
  RecordHistogramAction(HISTOGRAM_DISMISSED);
  button_pressed_ = true;
}

gfx::Image* AutoLoginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_AUTOLOGIN);
}

InfoBarDelegate::Type AutoLoginInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AutoLoginInfoBarDelegate::GetMessageText() const {
  return GetMessageText(params_.username);
}

string16 AutoLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOLOGIN_INFOBAR_OK_BUTTON : IDS_AUTOLOGIN_INFOBAR_CANCEL_BUTTON);
}

bool AutoLoginInfoBarDelegate::Accept() {
  // AutoLoginRedirector deletes itself.
  new AutoLoginRedirector(&owner()->GetWebContents()->GetController(),
                          params_.args);
  RecordHistogramAction(HISTOGRAM_ACCEPTED);
  button_pressed_ = true;
  return true;
}

bool AutoLoginInfoBarDelegate::Cancel() {
  PrefService* pref_service = TabContents::FromWebContents(
      owner()->GetWebContents())->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kAutologinEnabled, false);
  RecordHistogramAction(HISTOGRAM_REJECTED);
  button_pressed_ = true;
  return true;
}

string16 AutoLoginInfoBarDelegate::GetMessageText(
    const std::string& username) const {
  return l10n_util::GetStringFUTF16(IDS_AUTOLOGIN_INFOBAR_MESSAGE,
                                    UTF8ToUTF16(username));
}

void AutoLoginInfoBarDelegate::RecordHistogramAction(int action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Regular", action, HISTOGRAM_MAX);
}

void AutoLoginInfoBarDelegate::Observe(int type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_GOOGLE_SIGNED_OUT, type);
  // owner() can be NULL when InfoBarTabHelper removes us. See
  // |InfoBarDelegate::clear_owner|.
  if (owner())
    owner()->RemoveInfoBar(this);
}
