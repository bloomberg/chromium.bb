// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/one_click_signin_dialog.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/forms/password_form.h"
#include "webkit/forms/password_form_dom_manager.h"

// The infobar asking the user if they want to use one-click sign in.
class OneClickLoginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  OneClickLoginInfoBarDelegate(InfoBarTabHelper* owner,
                               const std::string& session_index,
                               const std::string& email,
                               const std::string& password);
  virtual ~OneClickLoginInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  virtual InfoBarAutomationType GetInfoBarAutomationType() const OVERRIDE;

  // Set the profile preference to turn off one-click sign in so that it won't
  // show again in this profile.
  void DisableOneClickSignIn();

  // Record the specified action in the histogram for one-click sign in.
  void RecordHistogramAction(int action);

  // Information about the account that has just logged in.
  std::string session_index_;
  std::string email_;
  std::string password_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(OneClickLoginInfoBarDelegate);
};

OneClickLoginInfoBarDelegate::OneClickLoginInfoBarDelegate(
    InfoBarTabHelper* owner,
    const std::string& session_index,
    const std::string& email,
    const std::string& password)
    : ConfirmInfoBarDelegate(owner),
      session_index_(session_index),
      email_(email),
      password_(password),
      button_pressed_(false) {
  RecordHistogramAction(one_click_signin::HISTOGRAM_SHOWN);
}

OneClickLoginInfoBarDelegate::~OneClickLoginInfoBarDelegate() {
  if (!button_pressed_)
    RecordHistogramAction(one_click_signin::HISTOGRAM_IGNORED);
}

void OneClickLoginInfoBarDelegate::InfoBarDismissed() {
  RecordHistogramAction(one_click_signin::HISTOGRAM_DISMISSED);
  button_pressed_ = true;
}

gfx::Image* OneClickLoginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_SYNC);
}

InfoBarDelegate::Type OneClickLoginInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 OneClickLoginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_INFOBAR_MESSAGE);
}

string16 OneClickLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      (button == BUTTON_OK) ? IDS_ONE_CLICK_SIGNIN_INFOBAR_OK_BUTTON
                            : IDS_ONE_CLICK_SIGNIN_INFOBAR_CANCEL_BUTTON);
}

namespace {

void OnLearnMore(Browser* browser) {
  browser->AddSelectedTabWithURL(
      GURL(chrome::kSyncLearnMoreURL),
      content::PAGE_TRANSITION_AUTO_BOOKMARK);
}

void OnAdvanced(Browser* browser) {
  browser->AddSelectedTabWithURL(
      GURL(std::string(chrome::kChromeUISettingsURL) +
           chrome::kSyncSetupSubPage),
      content::PAGE_TRANSITION_AUTO_BOOKMARK);
}

// Start syncing with the given user information.
void StartSync(content::WebContents* web_contents,
               const std::string& session_index,
               const std::string& email,
               const std::string& password,
               bool use_default_settings) {
  // The starter deletes itself once its done.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ignore_result(
      new OneClickSigninSyncStarter(
          profile, session_index, email, password, use_default_settings));

  Browser* browser = browser::FindBrowserWithWebContents(web_contents);
  browser->window()->ShowOneClickSigninBubble(
      base::Bind(&OnLearnMore, base::Unretained(browser)),
      base::Bind(&OnAdvanced, base::Unretained(browser)));
}

}  // namespace

bool OneClickLoginInfoBarDelegate::Accept() {
  DisableOneClickSignIn();
  RecordHistogramAction(one_click_signin::HISTOGRAM_ACCEPTED);
  ShowOneClickSigninDialog(
      owner()->web_contents()->GetView()->GetTopLevelNativeWindow(),
      base::Bind(&StartSync, owner()->web_contents(), session_index_, email_,
                 password_));
  button_pressed_ = true;
  return true;
}

bool OneClickLoginInfoBarDelegate::Cancel() {
  DisableOneClickSignIn();
  RecordHistogramAction(one_click_signin::HISTOGRAM_REJECTED);
  button_pressed_ = true;
  return true;
}

InfoBarDelegate::InfoBarAutomationType
    OneClickLoginInfoBarDelegate::GetInfoBarAutomationType() const {
  return ONE_CLICK_LOGIN_INFOBAR;
}

void OneClickLoginInfoBarDelegate::DisableOneClickSignIn() {
  PrefService* pref_service =
      TabContentsWrapper::GetCurrentWrapperForContents(
          owner()->web_contents())->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, false);
}

void OneClickLoginInfoBarDelegate::RecordHistogramAction(int action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse", action,
                            one_click_signin::HISTOGRAM_MAX);
}

// static
bool OneClickSigninHelper::CanOffer(content::WebContents* web_contents,
                                    bool check_connected) {
  if (!web_contents)
    return false;

  if (web_contents->GetBrowserContext()->IsOffTheRecord())
    return false;

  if (!ProfileSyncService::IsSyncEnabled())
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return false;

  if (!profile->GetPrefs()->GetBoolean(prefs::kReverseAutologinEnabled))
    return false;

  if (!SigninManager::AreSigninCookiesAllowed(profile))
    return false;

  if (check_connected) {
    SigninManager* manager =
        SigninManagerFactory::GetForProfile(profile);
    if (!manager)
      return false;

    if (!manager->GetAuthenticatedUsername().empty())
      return false;
  }

  return true;
}

// static
void OneClickSigninHelper::ShowInfoBarIfPossible(net::URLRequest* request,
                                                 int child_id,
                                                 int route_id) {
  // See if the response contains the Google-Accounts-SignIn header.
  std::string value;
  request->GetResponseHeaderByName("Google-Accounts-SignIn", &value);
  if (value.empty())
    return;

  std::vector<std::pair<std::string, std::string> > pairs;
  if (!base::SplitStringIntoKeyValuePairs(value, '=', ',', &pairs))
    return;

  // Parse the information from the value string.
  std::string email;
  std::string session_index;
  for (size_t i = 0; i < pairs.size(); ++i) {
    const std::pair<std::string, std::string>& pair = pairs[i];
    if (pair.first == "email") {
      TrimString(pair.second, "\"", &email);
    } else if (pair.first == "sessionindex") {
      session_index = pair.second;
    }
  }

  if (email.empty() || session_index.empty())
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OneClickSigninHelper::ShowInfoBarUIThread, session_index,
                 email, child_id, route_id));
}

OneClickSigninHelper::OneClickSigninHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

OneClickSigninHelper::~OneClickSigninHelper() {
}

// static
void OneClickSigninHelper::ShowInfoBarUIThread(
    const std::string& session_index,
    const std::string& email,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents = tab_util::GetWebContentsByID(child_id,
                                                                    route_id);
  if (!web_contents || !CanOffer(web_contents, true))
    return;

  // If some profile, not just the current one, is already connected to this
  // account, don't show the infobar.
  if (g_browser_process) {
    ProfileManager* manager = g_browser_process->profile_manager();
    if (manager) {
      string16 email16 = UTF8ToUTF16(email);
      ProfileInfoCache& cache = manager->GetProfileInfoCache();

      for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
        if (email16 == cache.GetUserNameOfProfileAtIndex(i))
          return;
      }
    }
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
  if (!wrapper)
    return;

  // Save the email in the one-click signin manager.  The manager may
  // not exist if the contents is incognito or if the profile is already
  // connected to a Google account.
  OneClickSigninHelper* helper = wrapper->one_click_signin_helper();
  if (helper)
    helper->SaveSessionIndexAndEmail(session_index, email);
}

void OneClickSigninHelper::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (params.password_form.origin.is_valid())
    SavePassword(UTF16ToUTF8(params.password_form.password_value));
}

void OneClickSigninHelper::DidStopLoading() {
  if (email_.empty() || password_.empty())
    return;

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents());

  wrapper->infobar_tab_helper()->AddInfoBar(
      new OneClickLoginInfoBarDelegate(wrapper->infobar_tab_helper(),
                                       session_index_, email_, password_));

  email_.clear();
  password_.clear();
}

void OneClickSigninHelper::SaveSessionIndexAndEmail(
    const std::string& session_index,
    const std::string& email) {
  session_index_ = session_index;
  email_ = email;
}

void OneClickSigninHelper::SavePassword(const std::string& password) {
  // TODO(rogerta): in the case of a 2-factor or captcha or some other type of
  // challenge, its possible for the user to never complete the signin.
  // Should have a way to detect this and clear the password member.
  password_ = password;
}
