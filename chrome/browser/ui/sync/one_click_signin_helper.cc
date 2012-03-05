// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_helper.h"

#include "base/metrics/histogram.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/sync/one_click_signin_dialog.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/forms/password_form.h"
#include "webkit/forms/password_form_dom_manager.h"

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

// The infobar asking the user if they want to use one-click sign in.
class OneClickLoginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  OneClickLoginInfoBarDelegate(InfoBarTabHelper* owner,
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

  void RecordHistogramAction(int action);

  Profile* profile_;

  // Email address and password of the account that has just logged in.
  std::string email_;
  std::string password_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(OneClickLoginInfoBarDelegate);
};

OneClickLoginInfoBarDelegate::OneClickLoginInfoBarDelegate(
    InfoBarTabHelper* owner,
    const std::string& email,
    const std::string& password)
    : ConfirmInfoBarDelegate(owner),
      profile_(Profile::FromBrowserContext(
          owner->web_contents()->GetBrowserContext())),
      email_(email),
      password_(password),
      button_pressed_(false) {
  DCHECK(profile_);
  RecordHistogramAction(HISTOGRAM_SHOWN);
}

OneClickLoginInfoBarDelegate::~OneClickLoginInfoBarDelegate() {
  if (!button_pressed_)
    RecordHistogramAction(HISTOGRAM_IGNORED);
}

void OneClickLoginInfoBarDelegate::InfoBarDismissed() {
  RecordHistogramAction(HISTOGRAM_DISMISSED);
  button_pressed_ = true;
}

gfx::Image* OneClickLoginInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_AUTOLOGIN);
}

InfoBarDelegate::Type OneClickLoginInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 OneClickLoginInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_INFOBAR_MESSAGE);
}

string16 OneClickLoginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_OK : IDS_ONE_CLICK_SIGNIN_INFOBAR_CANCEL_BUTTON);
}

bool OneClickLoginInfoBarDelegate::Accept() {
  RecordHistogramAction(HISTOGRAM_ACCEPTED);
  ShowOneClickSigninDialog(profile_, email_, password_);
  button_pressed_ = true;
  return true;
}

bool OneClickLoginInfoBarDelegate::Cancel() {
  PrefService* pref_service =
      TabContentsWrapper::GetCurrentWrapperForContents(
          owner()->web_contents())->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, false);
  RecordHistogramAction(HISTOGRAM_REJECTED);
  button_pressed_ = true;
  return true;
}

void OneClickLoginInfoBarDelegate::RecordHistogramAction(int action) {
  UMA_HISTOGRAM_ENUMERATION("AutoLogin.Reverse", action, HISTOGRAM_MAX);
}

}  // namespace

// static
bool OneClickSigninHelper::CanOffer(content::WebContents* web_contents) {
  return !web_contents->GetBrowserContext()->IsOffTheRecord();
}

// static
void OneClickSigninHelper::ShowInfoBarIfPossible(net::URLRequest* request,
                                                 int child_id,
                                                 int route_id) {
  // See if the response contains the X-Google-Accounts-SignIn header.
  std::string value;
  request->GetResponseHeaderByName("X-Google-Accounts-SignIn", &value);
  if (value.empty())
    return;

  std::vector<std::pair<std::string, std::string> > pairs;
  if (!base::SplitStringIntoKeyValuePairs(value, '=', ',', &pairs))
    return;

  // Parse the information from the value string.
  std::string email;
  for (size_t i = 0; i < pairs.size(); ++i) {
    const std::pair<std::string, std::string>& pair = pairs[i];
    if (pair.first == "email")
      TrimString(pair.second, "\"", &email);
  }

  if (email.empty())
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OneClickSigninHelper::ShowInfoBarUIThread, email,
                 child_id, route_id));
}

OneClickSigninHelper::OneClickSigninHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

OneClickSigninHelper::~OneClickSigninHelper() {
}

// static
void OneClickSigninHelper::ShowInfoBarUIThread(
    const std::string& email,
    int child_id,
    int route_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents = tab_util::GetWebContentsByID(child_id,
                                                                    route_id);
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (!ProfileSyncService::IsSyncEnabled())
    return;

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  if (!profile->GetPrefs()->GetBoolean(prefs::kReverseAutologinEnabled) ||
      service->AreCredentialsAvailable())
    return;

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
  if (!wrapper)
    return;

  // TODO(rogerta): remove this #if once the dialog is fully implemented for
  // mac and linux.
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // Save the email in the one-click signin manager.  The manager may
  // not exist if the contents is incognito or if the profile is already
  // connected to a Google account.
  OneClickSigninHelper* helper = wrapper->one_click_signin_helper();
  if (helper)
    helper->SaveEmail(email);
#endif
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
                                       email_, password_));

  email_.clear();
  password_.clear();
}

void OneClickSigninHelper::SaveEmail(const std::string& email) {
  // TODO(rogerta): validate that the email address is the same as set in
  // the form?
  email_ = email;
}

void OneClickSigninHelper::SavePassword(const std::string& password) {
  // TODO(rogerta): in the case of a 2-factor or captcha or some other type of
  // challenge, its possible for the user to never complete the signin.
  // Should have a way to detect this and clear the password member.
  password_ = password;
}
