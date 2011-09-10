// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autologin_infobar_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// This class holds context information needed while re-issuing service tokens
// using the TokenService, getting the browser cookies with the TokenAuth API,
// and finally redirecting the user to the correct page.
class AutoLoginRedirector : public NotificationObserver {
 public:
  AutoLoginRedirector(TabContentsWrapper* tab_contents_wrapper,
                      const std::string& args);
  virtual ~AutoLoginRedirector();

 private:
  // NotificationObserver override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Redirect tab to MergeSession URL, logging the user in and navigating
  // to the desired page.
  void RedirectToMergeSession(const std::string& token);

  TabContentsWrapper* tab_contents_wrapper_;
  const std::string args_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginRedirector);
};

AutoLoginRedirector::AutoLoginRedirector(
    TabContentsWrapper* tab_contents_wrapper, const std::string& args)
    : tab_contents_wrapper_(tab_contents_wrapper), args_(args) {
  // Register to receive notification for new tokens and then force the tokens
  // to be re-issued.  The token service guarantees to fire either
  // TOKEN_AVAILABLE or TOKEN_REQUEST_FAILED, so we will get at least one or
  // the other, allow AutoLoginRedirector to delete itself correctly.
  TokenService* service = tab_contents_wrapper_->profile()->GetTokenService();
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 Source<TokenService>(service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 Source<TokenService>(service));
  service->StartFetchingTokens();
}

AutoLoginRedirector::~AutoLoginRedirector() {
}

void AutoLoginRedirector::Observe(int type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  // We are only interested in GAIA tokens.
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService::TokenAvailableDetails* tok_details =
        Details<TokenService::TokenAvailableDetails>(details).ptr();

    if (tok_details->service() == GaiaConstants::kGaiaService) {
      RedirectToMergeSession(tok_details->token());
      delete this;
      return;
    }
  } else {
    TokenService::TokenRequestFailedDetails* tok_details =
        Details<TokenService::TokenRequestFailedDetails>(details).ptr();

    if (tok_details->service() == GaiaConstants::kGaiaService) {
      LOG(WARNING) << "AutoLoginRedirector: token request failed";
      delete this;
      return;
    }
  }
}

void AutoLoginRedirector::RedirectToMergeSession(const std::string& token) {
  // The args are URL encoded, so we need to decode them before use.
  url_canon::RawCanonOutputT<char16> output;
  url_util::DecodeURLEscapeSequences(args_.c_str(), args_.length(), &output);
  std::string unescaped_args;
  UTF16ToUTF8(output.data(), output.length(), &unescaped_args);

  const char kUrlFormat[] = "%s?"
                            "source=chrome&"
                            "uberauth=%s&"
                            "%s";
  const std::string url_string = GaiaUrls::GetInstance()->merge_session_url();
  std::string url = base::StringPrintf(kUrlFormat,
                                       url_string.c_str(),
                                       token.c_str(),
                                       unescaped_args.c_str());

  // TODO(rogerta): what is the correct page transition?
  tab_contents_wrapper_->tab_contents()->controller().LoadURL(GURL(url),
      GURL(), PageTransition::AUTO_BOOKMARK, std::string());
}

// static
void AutoLoginInfoBarDelegate::ShowIfAutoLoginRequested(
    net::URLRequest* request,
    int child_id,
    int route_id) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutologin)) {
    return;
  }

  // See if the response contains the X-Auto-Login header.  If so, this was
  // a request for a login page, and the server is allowing the browser to
  // suggest auto-login, if available.
  std::string value;
  request->GetResponseHeaderByName("X-Auto-Login", &value);
  if (value.empty())
    return;

  std::vector<std::pair<std::string, std::string> > pairs;
  if (!base::SplitStringIntoKeyValuePairs(value, '=', '&', &pairs))
    return;

  // Parse the information from the value string.
  std::string realm;
  std::string account;
  std::string args;
  for (size_t i = 0; i < pairs.size(); ++i) {
    const std::pair<std::string, std::string>& pair = pairs[i];
    if (pair.first == "realm") {
      realm = pair.second;
    } else if (pair.first == "account") {
      account = pair.second;
    } else if (pair.first == "args") {
      args = pair.second;
    }
  }

  // We only accept GAIA credentdials.
  if (realm != "com.google")
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&AutoLoginInfoBarDelegate::ShowInfoBarIfNeeded,
                          account, args, child_id, route_id));
}

// static
void AutoLoginInfoBarDelegate::ShowInfoBarIfNeeded(const std::string& account,
                                                   const std::string& args,
                                                   int child_id, int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* tab_contents = tab_util::GetTabContentsByID(child_id, route_id);
  if (!tab_contents)
    return;

  TabContentsWrapper* tab_contents_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  // tab_contents_wrapper is NULL for TabContents hosted in HTMLDialog.
  if (!tab_contents_wrapper)
    return;

  // If auto-login is turned off, then simply return.
  if (!tab_contents_wrapper->profile()->GetPrefs()->GetBoolean(
      prefs::kAutologinEnabled))
    return;

  // Make sure that the account specified matches the logged in user.
  // However, account is usually empty.  In an incognito window, there may
  // not be a profile sync service and/or signin manager.
  if (!tab_contents_wrapper->profile()->HasProfileSyncService())
    return;

  SigninManager* signin_manager =
      tab_contents_wrapper->profile()->GetProfileSyncService()->signin();
  if (!signin_manager)
    return;

  const std::string& username = signin_manager->GetUsername();
  if (!account.empty() && (username != account))
    return;

  // Make sure there are credentials in the token manager, otherwise there is
  // no way to craft the TokenAuth URL.
  if (!tab_contents_wrapper->profile()->GetTokenService()->
      AreCredentialsValid()) {
    return;
  }

  // We can't add the infobar just yet, since we need to wait for the tab
  // to finish loading.  If we don't, the info bar appears and then disappears
  // immediately.  The delegate will eventually be deleted by the TabContents.
  new AutoLoginInfoBarDelegate(tab_contents_wrapper, username, args);
}

AutoLoginInfoBarDelegate::AutoLoginInfoBarDelegate(
    TabContentsWrapper* tab_contents_wrapper, const std::string& account,
    const std::string& args)
    : ConfirmInfoBarDelegate(tab_contents_wrapper->tab_contents()),
      tab_contents_wrapper_(tab_contents_wrapper),
      account_(account), args_(args) {
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
          &tab_contents_wrapper->tab_contents()->controller()));
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
      Source<TabContents>(tab_contents_wrapper->tab_contents()));
}

AutoLoginInfoBarDelegate::~AutoLoginInfoBarDelegate() {
}

void AutoLoginInfoBarDelegate::Observe(int type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    // The wrapper takes ownership of this delegate.
    tab_contents_wrapper_->infobar_tab_helper()->AddInfoBar(this);
    registrar_.RemoveAll();
  } else if (type == content::NOTIFICATION_TAB_CONTENTS_DESTROYED) {
    // The tab contents was destroyed before the naviagation completed, so
    // just cleanup this delegate.
    delete this;
  } else {
    NOTREACHED() << "Unexpected notification type";
  }
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
                                    UTF8ToUTF16(account_));
}

int AutoLoginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 AutoLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(BUTTON_OK == button ?
      IDS_AUTOLOGIN_INFOBAR_OK_BUTTON : IDS_AUTOLOGIN_INFOBAR_CANCEL_BUTTON);
}

bool AutoLoginInfoBarDelegate::Accept() {
  // AutoLoginRedirector auto deletes itself.
  new AutoLoginRedirector(tab_contents_wrapper_, args_);
  return true;
}

bool AutoLoginInfoBarDelegate::Cancel() {
  PrefService* user_prefs = tab_contents_wrapper_->profile()->GetPrefs();
  user_prefs->SetBoolean(prefs::kAutologinEnabled, false);
  user_prefs->ScheduleSavePersistentPrefs();
  return true;
}
