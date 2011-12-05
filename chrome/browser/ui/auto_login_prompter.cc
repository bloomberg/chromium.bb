// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/auto_login_prompter.h"

#include "base/bind.h"
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
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

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

// This is the actual infobar displayed to prompt the user to auto-login.
class AutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  AutoLoginInfoBarDelegate(InfoBarTabHelper* owner,
                           NavigationController* navigation_controller,
                           TokenService* token_service,
                           PrefService* pref_service,
                           const std::string& username,
                           const std::string& args);
  virtual ~AutoLoginInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  NavigationController* navigation_controller_;
  TokenService* token_service_;
  PrefService* pref_service_;
  std::string username_;
  std::string args_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

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
  pref_service_->ScheduleSavePersistentPrefs();
  return true;
}


// AutoLoginPrompter ----------------------------------------------------------

AutoLoginPrompter::AutoLoginPrompter(
    TabContents* tab_contents,
    const std::string& username,
    const std::string& args)
    : tab_contents_(tab_contents),
      username_(username),
      args_(args) {
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(
                    &tab_contents_->controller()));
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 content::Source<TabContents>(tab_contents_));
}

AutoLoginPrompter::~AutoLoginPrompter() {
}

// static
void AutoLoginPrompter::ShowInfoBarIfPossible(net::URLRequest* request,
                                              int child_id,
                                              int route_id) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAutologin))
    return;

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

  // Currently we only accept GAIA credentials.
  if (realm != "com.google")
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AutoLoginPrompter::ShowInfoBarUIThread, account, args,
                 child_id, route_id));
}

// static
void AutoLoginPrompter::ShowInfoBarUIThread(const std::string& account,
                                            const std::string& args,
                                            int child_id,
                                            int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* tab_contents = tab_util::GetTabContentsByID(child_id, route_id);
  if (!tab_contents)
    return;

  // If auto-login is turned off, then simply return.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents->browser_context());
  if (!profile->GetPrefs()->GetBoolean(prefs::kAutologinEnabled))
    return;

  // In an incognito window, there may not be a profile sync service and/or
  // signin manager.
  if (!profile->HasProfileSyncService())
    return;
  SigninManager* signin_manager = profile->GetProfileSyncService()->signin();
  if (!signin_manager)
    return;

  // Make sure that |account|, if specified, matches the logged in user.
  // However, |account| is usually empty.
  const std::string& username = signin_manager->GetUsername();
  if (!account.empty() && (username != account))
    return;

  // Make sure there are credentials in the token manager, otherwise there is
  // no way to craft the TokenAuth URL.
  if (!profile->GetTokenService()->AreCredentialsValid())
    return;

  // We can't add the infobar just yet, since we need to wait for the tab to
  // finish loading.  If we don't, the info bar appears and then disappears
  // immediately.  Create an AutoLoginPrompter instance to listen for the
  // relevant notifications; it will delete itself.
  new AutoLoginPrompter(tab_contents, username, args);
}

void AutoLoginPrompter::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(tab_contents_);
    // |wrapper| is NULL for TabContents hosted in HTMLDialog.
    if (wrapper) {
      InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
      Profile* profile = wrapper->profile();
      infobar_helper->AddInfoBar(new AutoLoginInfoBarDelegate(
          infobar_helper, &tab_contents_->controller(),
          profile->GetTokenService(), profile->GetPrefs(),
          username_, args_));
    }
  }
  // Either we couldn't add the infobar, we added the infobar, or the tab
  // contents was destroyed before the navigation completed.  In any case
  // there's no reason to live further.
  delete this;
}
