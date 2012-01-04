// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_info_bar_delegate.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::NavigationController;

// AutoLoginRedirector --------------------------------------------------------

// This class is created by the AutoLoginInfoBarDelegate when the user wishes to
// auto-login.  It holds context information needed while re-issuing service
// tokens using the TokenService, gets the browser cookies with the TokenAuth
// API, and finally redirects the user to the correct page.
class AutoLoginRedirector : public content::NotificationObserver {
 public:
  AutoLoginRedirector(TokenService* token_service,
                      NavigationController* navigation_controller,
                      const std::string& args);
  virtual ~AutoLoginRedirector();

 private:
  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Redirect tab to MergeSession URL, logging the user in and navigating
  // to the desired page.
  void RedirectToMergeSession(const std::string& token);

  NavigationController* navigation_controller_;
  const std::string args_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginRedirector);
};

AutoLoginRedirector::AutoLoginRedirector(
    TokenService* token_service,
    NavigationController* navigation_controller,
    const std::string& args)
    : navigation_controller_(navigation_controller),
      args_(args) {
  // Register to receive notification for new tokens and then force the tokens
  // to be re-issued.  The token service guarantees to fire either
  // TOKEN_AVAILABLE or TOKEN_REQUEST_FAILED, so we will get at least one or
  // the other, allow AutoLoginRedirector to delete itself correctly.
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 content::Source<TokenService>(token_service));
  token_service->StartFetchingTokens();
}

AutoLoginRedirector::~AutoLoginRedirector() {
}

void AutoLoginRedirector::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  // We are only interested in GAIA tokens.
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService::TokenAvailableDetails* tok_details =
        content::Details<TokenService::TokenAvailableDetails>(details).ptr();
    if (tok_details->service() == GaiaConstants::kGaiaService) {
      RedirectToMergeSession(tok_details->token());
      delete this;
    }
  } else {
    TokenService::TokenRequestFailedDetails* tok_details =
        content::Details<TokenService::TokenRequestFailedDetails>(details).
            ptr();
    if (tok_details->service() == GaiaConstants::kGaiaService) {
      LOG(WARNING) << "AutoLoginRedirector: token request failed";
      delete this;
    }
  }
}

void AutoLoginRedirector::RedirectToMergeSession(const std::string& token) {
  // The args are URL encoded, so we need to decode them before use.
  url_canon::RawCanonOutputT<char16> output;
  url_util::DecodeURLEscapeSequences(args_.c_str(), args_.length(), &output);
  std::string unescaped_args;
  UTF16ToUTF8(output.data(), output.length(), &unescaped_args);
  // TODO(rogerta): what is the correct page transition?
  navigation_controller_->LoadURL(
      GURL(GaiaUrls::GetInstance()->merge_session_url() +
          "?source=chrome&uberauth=" + token + "&" + unescaped_args),
      content::Referrer(), content::PAGE_TRANSITION_AUTO_BOOKMARK,
      std::string());
}


// AutoLoginInfoBarDelegate ---------------------------------------------------

AutoLoginInfoBarDelegate::AutoLoginInfoBarDelegate(
    InfoBarTabHelper* owner,
    NavigationController* navigation_controller,
    TokenService* token_service,
    PrefService* pref_service,
    const std::string& username,
    const std::string& args)
    : ConfirmInfoBarDelegate(owner),
      navigation_controller_(navigation_controller),
      token_service_(token_service),
      pref_service_(pref_service),
      username_(username),
      args_(args) {
}

AutoLoginInfoBarDelegate::~AutoLoginInfoBarDelegate() {
}

gfx::Image* AutoLoginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_AUTOLOGIN);
}

InfoBarDelegate::Type AutoLoginInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 AutoLoginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_AUTOLOGIN_INFOBAR_MESSAGE,
                                    UTF8ToUTF16(username_));
}

string16 AutoLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_AUTOLOGIN_INFOBAR_OK_BUTTON : IDS_AUTOLOGIN_INFOBAR_CANCEL_BUTTON);
}

bool AutoLoginInfoBarDelegate::Accept() {
  // AutoLoginRedirector deletes itself.
  new AutoLoginRedirector(token_service_, navigation_controller_, args_);
  return true;
}

bool AutoLoginInfoBarDelegate::Cancel() {
  pref_service_->SetBoolean(prefs::kAutologinEnabled, false);
  return true;
}
