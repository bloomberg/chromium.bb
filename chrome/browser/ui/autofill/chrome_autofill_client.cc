// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/credit_card_scanner_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_cc_infobar_delegate.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chromium_application.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#else
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/ui/zoom/zoom_controller.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::ChromeAutofillClient);

namespace autofill {

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      unmask_controller_(web_contents),
      last_rfh_to_rac_(nullptr) {
  DCHECK(web_contents);

#if !defined(OS_ANDROID)
  // Since ZoomController is also a WebContentsObserver, we need to be careful
  // about disconnecting from it since the relative order of destruction of
  // WebContentsObservers is not guaranteed. ZoomController silently clears
  // its ZoomObserver list during WebContentsDestroyed() so there's no need
  // to explicitly remove ourselves on destruction.
  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents);
  // There may not always be a ZoomController, e.g. in tests.
  if (zoom_controller)
    zoom_controller->AddObserver(this);
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
  RegisterForKeystoneNotifications();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

ChromeAutofillClient::~ChromeAutofillClient() {
  // NOTE: It is too late to clean up the autofill popup; that cleanup process
  // requires that the WebContents instance still be valid and it is not at
  // this point (in particular, the WebContentsImpl destructor has already
  // finished running and we are now in the base class destructor).
  DCHECK(!popup_controller_);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  UnregisterFromKeystoneNotifications();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
}

void ChromeAutofillClient::TabActivated() {
  if (dialog_controller_.get())
    dialog_controller_->TabActivated();
}

PersonalDataManager* ChromeAutofillClient::GetPersonalDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

scoped_refptr<AutofillWebDataService> ChromeAutofillClient::GetDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
}

PrefService* ChromeAutofillClient::GetPrefs() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext())
      ->GetPrefs();
}

IdentityProvider* ChromeAutofillClient::GetIdentityProvider() {
  if (!identity_provider_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    LoginUIService* login_service = nullptr;
#if !defined(OS_ANDROID)
    login_service = LoginUIServiceFactory::GetForProfile(profile);
#endif
    identity_provider_.reset(new ProfileIdentityProvider(
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        login_service));
  }

  return identity_provider_.get();
}

rappor::RapporService* ChromeAutofillClient::GetRapporService() {
  return g_browser_process->rappor_service();
}

void ChromeAutofillClient::ShowAutofillSettings() {
#if defined(OS_ANDROID)
  chrome::android::ChromiumApplication::ShowAutofillSettings();
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser)
    chrome::ShowSettingsSubPage(browser, chrome::kAutofillSubPage);
#endif  // #if defined(OS_ANDROID)
}

void ChromeAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(card, delegate);
}

void ChromeAutofillClient::OnUnmaskVerificationResult(GetRealPanResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClient::ConfirmSaveCreditCard(
    const base::Closure& save_card_callback) {
  AutofillCCInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents()), this,
      save_card_callback);
}

bool ChromeAutofillClient::HasCreditCardScanFeature() {
  return CreditCardScannerController::HasCreditCardScanFeature();
}

void ChromeAutofillClient::ScanCreditCard(
    const CreditCardScanCallback& callback) {
  CreditCardScannerController::ScanCreditCard(web_contents(), callback);
}

void ChromeAutofillClient::ShowRequestAutocompleteDialog(
    const FormData& form,
    content::RenderFrameHost* render_frame_host,
    const ResultCallback& callback) {
  HideRequestAutocompleteDialog();
  last_rfh_to_rac_ = render_frame_host;
  GURL frame_url = render_frame_host->GetLastCommittedURL();
  dialog_controller_ = AutofillDialogController::Create(web_contents(), form,
                                                        frame_url, callback);
  if (dialog_controller_) {
    dialog_controller_->Show();
  } else {
    callback.Run(AutofillClient::AutocompleteResultErrorDisabled,
                 base::string16(),
                 NULL);
    NOTIMPLEMENTED();
  }
}

void ChromeAutofillClient::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<autofill::Suggestion>& suggestions,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();

  // Will delete or reuse the old |popup_controller_|.
  popup_controller_ =
      AutofillPopupControllerImpl::GetOrCreate(popup_controller_,
                                               delegate,
                                               web_contents(),
                                               web_contents()->GetNativeView(),
                                               element_bounds_in_screen_space,
                                               text_direction);

  popup_controller_->Show(suggestions);
}

void ChromeAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  if (popup_controller_.get())
    popup_controller_->UpdateDataListValues(values, labels);
}

void ChromeAutofillClient::HideAutofillPopup() {
  if (popup_controller_.get())
    popup_controller_->Hide();

  // Password generation popups behave in the same fashion and should also
  // be hidden.
  ChromePasswordManagerClient* password_client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  if (password_client)
    password_client->HidePasswordGenerationPopup();
}

bool ChromeAutofillClient::IsAutocompleteEnabled() {
  // For browser, Autocomplete is always enabled as part of Autofill.
  return GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void ChromeAutofillClient::HideRequestAutocompleteDialog() {
  if (dialog_controller_)
    dialog_controller_->Hide();
}

void ChromeAutofillClient::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (dialog_controller_ && render_frame_host == last_rfh_to_rac_)
    HideRequestAutocompleteDialog();
}

void ChromeAutofillClient::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (dialog_controller_ && render_frame_host == last_rfh_to_rac_)
    HideRequestAutocompleteDialog();
}

void ChromeAutofillClient::MainFrameWasResized(bool width_changed) {
#if defined(OS_ANDROID)
  // Ignore virtual keyboard showing and hiding a strip of suggestions.
  if (!width_changed)
    return;
#endif

  HideAutofillPopup();
}

void ChromeAutofillClient::WebContentsDestroyed() {
  HideAutofillPopup();
}

void ChromeAutofillClient::OnZoomChanged(
    const ui_zoom::ZoomController::ZoomChangedEventData& data) {
  HideAutofillPopup();
}

void ChromeAutofillClient::DetectAccountCreationForms(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  if (driver)
    driver->GetPasswordGenerationManager()->DetectAccountCreationForms(forms);
}

void ChromeAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
#if defined(OS_ANDROID)
  AutofillLoggerAndroid::DidFillOrPreviewField(autofilled_value,
                                               profile_full_name);
#endif  // defined(OS_ANDROID)
}

void ChromeAutofillClient::OnFirstUserGestureObserved() {
  web_contents()->SendToAllFrames(
      new AutofillMsg_FirstUserGestureObservedInTab(routing_id()));
}

void ChromeAutofillClient::LinkClicked(const GURL& url,
                                       WindowOpenDisposition disposition) {
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), disposition, ui::PAGE_TRANSITION_LINK, false));
}

}  // namespace autofill
