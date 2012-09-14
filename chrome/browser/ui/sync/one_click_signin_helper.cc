// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/metrics/field_trial.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/one_click_signin_infobar_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_version_info.h"
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
#include "grit/theme_resources.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/forms/password_form.h"
#include "webkit/forms/password_form_dom_manager.h"

namespace {

// Set to true if this chrome instance is in the blue-button-on-white-bar
// experimental group.
bool use_blue_on_white = false;

// Start syncing with the given user information.
void StartSync(content::WebContents* web_contents,
               const std::string& session_index,
               const std::string& email,
               const std::string& password,
               OneClickSigninSyncStarter::StartSyncMode start_mode) {
  // The starter deletes itself once its done.
  Browser* browser = browser::FindBrowserWithWebContents(web_contents);
  new OneClickSigninSyncStarter(browser, session_index, email, password,
                                start_mode);
}

}  // namespace

// The infobar asking the user if they want to use one-click sign in.
class OneClickInfoBarDelegateImpl : public OneClickSigninInfoBarDelegate {
 public:
  OneClickInfoBarDelegateImpl(InfoBarTabHelper* owner,
                               const std::string& session_index,
                               const std::string& email,
                               const std::string& password);
  virtual ~OneClickInfoBarDelegateImpl();

 private:
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

  // Set the profile preference to turn off one-click sign in so that it won't
  // show again in this profile.
  void DisableOneClickSignIn();

  // Add a specific email to the list of emails rejected for one-click
  // sign-in, for this profile.
  void AddEmailToOneClickRejectedList(const std::string& email);

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

OneClickInfoBarDelegateImpl::OneClickInfoBarDelegateImpl(
    InfoBarTabHelper* owner,
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
  // User has accepted one-click sign-in for this account. Never ask again for
  // this profile.
  DisableOneClickSignIn();
  content::WebContents* web_contents = owner()->GetWebContents();
  RecordHistogramAction(one_click_signin::HISTOGRAM_ACCEPTED);
  browser::FindBrowserWithWebContents(web_contents)->window()->
      ShowOneClickSigninBubble(base::Bind(&StartSync, web_contents,
                                          session_index_, email_, password_));
  button_pressed_ = true;
  return true;
}

bool OneClickInfoBarDelegateImpl::Cancel() {
  AddEmailToOneClickRejectedList(email_);
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

void OneClickInfoBarDelegateImpl::DisableOneClickSignIn() {
  PrefService* pref_service =
      TabContents::FromWebContents(owner()->GetWebContents())->
          profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, false);
}

void OneClickInfoBarDelegateImpl::AddEmailToOneClickRejectedList(
    const std::string& email) {
  PrefService* pref_service =
      TabContents::FromWebContents(owner()->GetWebContents())->
          profile()->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(base::Value::CreateStringValue(email));
}

void OneClickInfoBarDelegateImpl::RecordHistogramAction(int action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse", action,
                            one_click_signin::HISTOGRAM_MAX);
}

OneClickSigninHelper::OneClickSigninHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

OneClickSigninHelper::~OneClickSigninHelper() {
}

// static
bool OneClickSigninHelper::CanOffer(content::WebContents* web_contents,
                                    const std::string& email,
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

    // Make sure this username is not prohibited by policy.
    if (!manager->IsAllowedUsername(email))
      return false;

    // If some profile, not just the current one, is already connected to this
    // account, don't show the infobar.
    if (g_browser_process) {
      ProfileManager* manager = g_browser_process->profile_manager();
      if (manager) {
        string16 email16 = UTF8ToUTF16(email);
        ProfileInfoCache& cache = manager->GetProfileInfoCache();

        for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i) {
          if (email16 == cache.GetUserNameOfProfileAtIndex(i))
            return false;
        }
      }
    }

    // If email was already rejected by this profile for one-click sign-in.
    if (!email.empty()) {
      const ListValue* rejected_emails = profile->GetPrefs()->GetList(
          prefs::kReverseAutologinRejectedEmailList);
      if (!rejected_emails->empty()) {
        const scoped_ptr<Value> email_value(Value::CreateStringValue(email));
        ListValue::const_iterator iter = rejected_emails->Find(
            *email_value);
        if (iter != rejected_emails->end())
          return false;
      }
    }
  }

  return true;
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

// static
void OneClickSigninHelper::ShowInfoBarUIThread(
    const std::string& session_index,
    const std::string& email,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents = tab_util::GetWebContentsByID(child_id,
                                                                    route_id);

  // TODO(mathp): The appearance of this infobar should be tested using a
  // browser_test.
  if (!web_contents || !CanOffer(web_contents, email, true))
    return;

  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  if (!tab_contents)
    return;

  // Save the email in the one-click signin manager.  The manager may
  // not exist if the contents is incognito or if the profile is already
  // connected to a Google account.
  OneClickSigninHelper* helper = tab_contents->one_click_signin_helper();
  if (helper)
    helper->SaveSessionIndexAndEmail(session_index, email);
}

void OneClickSigninHelper::DidNavigateAnyFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (params.password_form.origin.is_valid())
    SavePassword(UTF16ToUTF8(params.password_form.password_value));
}

void OneClickSigninHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  if (email_.empty() || password_.empty())
    return;

  TabContents* tab_contents = TabContents::FromWebContents(web_contents());

  tab_contents->infobar_tab_helper()->AddInfoBar(
      new OneClickInfoBarDelegateImpl(tab_contents->infobar_tab_helper(),
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
