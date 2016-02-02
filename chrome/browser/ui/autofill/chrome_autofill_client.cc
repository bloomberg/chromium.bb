// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/credit_card_scanner_controller.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_cc_infobar_delegate.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/chrome_application.h"
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#else
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/ui/zoom/zoom_controller.h"
#endif

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/infobars/core/infobar.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::ChromeAutofillClient);

namespace autofill {

namespace {

#if !defined(OS_ANDROID)
bool IsSaveCardBubbleEnabled() {
#if defined(OS_MACOSX)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSaveCardBubble);
#else
  return true;
#endif
}
#endif  // !defined(OS_ANDROID)

}  // namespace

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      unmask_controller_(
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext()),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->IsOffTheRecord()),
      last_rfh_to_rac_(nullptr) {
  DCHECK(web_contents);

#if !BUILDFLAG(ANDROID_JAVA_UI)
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
}

ChromeAutofillClient::~ChromeAutofillClient() {
  // NOTE: It is too late to clean up the autofill popup; that cleanup process
  // requires that the WebContents instance still be valid and it is not at
  // this point (in particular, the WebContentsImpl destructor has already
  // finished running and we are now in the base class destructor).
  DCHECK(!popup_controller_);
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

sync_driver::SyncService* ChromeAutofillClient::GetSyncService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
}

IdentityProvider* ChromeAutofillClient::GetIdentityProvider() {
  if (!identity_provider_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetOriginalProfile();
    base::Closure login_callback;
#if !BUILDFLAG(ANDROID_JAVA_UI)
    login_callback =
        LoginUIServiceFactory::GetShowLoginPopupCallbackForProfile(profile);
#endif
    identity_provider_.reset(new ProfileIdentityProvider(
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        login_callback));
  }

  return identity_provider_.get();
}

rappor::RapporService* ChromeAutofillClient::GetRapporService() {
  return g_browser_process->rappor_service();
}

void ChromeAutofillClient::ShowAutofillSettings() {
#if BUILDFLAG(ANDROID_JAVA_UI)
  chrome::android::ChromeApplication::ShowAutofillSettings();
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser)
    chrome::ShowSettingsSubPage(browser, chrome::kAutofillSubPage);
#endif  // #if BUILDFLAG(ANDROID_JAVA_UI)
}

void ChromeAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      CreateCardUnmaskPromptView(&unmask_controller_, web_contents()),
      card, delegate);
}

void ChromeAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    const base::Closure& callback) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  InfoBarService::FromWebContents(web_contents())->AddInfoBar(
      CreateSaveCardInfoBarMobile(
          make_scoped_ptr(new AutofillSaveCardInfoBarDelegateMobile(
              false, card, scoped_ptr<base::DictionaryValue>(nullptr),
              callback))));
#else
  if (IsSaveCardBubbleEnabled()) {
    // Do lazy initialization of SaveCardBubbleControllerImpl.
    autofill::SaveCardBubbleControllerImpl::CreateForWebContents(
        web_contents());
    autofill::SaveCardBubbleControllerImpl* controller =
        autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
    controller->ShowBubbleForLocalSave(card, callback);
    return;
  }

  AutofillCCInfoBarDelegate::CreateForLocalSave(
      InfoBarService::FromWebContents(web_contents()), callback);
#endif
}

void ChromeAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    scoped_ptr<base::DictionaryValue> legal_message,
    const base::Closure& callback) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  InfoBarService::FromWebContents(web_contents())->AddInfoBar(
      CreateSaveCardInfoBarMobile(
          make_scoped_ptr(new AutofillSaveCardInfoBarDelegateMobile(
              true, card, std::move(legal_message), callback))));
#else
  // Do lazy initialization of SaveCardBubbleControllerImpl.
  autofill::SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  controller->ShowBubbleForUpload(card, std::move(legal_message), callback);
#endif
}

void ChromeAutofillClient::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {
  ::autofill::LoadRiskData(0, web_contents(), callback);
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
#if BUILDFLAG(ANDROID_JAVA_UI)
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

void ChromeAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {
  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          rfh);
  if (driver) {
    driver->GetPasswordGenerationManager()->DetectFormsEligibleForGeneration(
        forms);
    driver->GetPasswordManager()->ProcessAutofillPredictions(driver, forms);
  }
}

void ChromeAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
#if BUILDFLAG(ANDROID_JAVA_UI)
  AutofillLoggerAndroid::DidFillOrPreviewField(autofilled_value,
                                               profile_full_name);
#endif  // BUILDFLAG(ANDROID_JAVA_UI)
}

void ChromeAutofillClient::OnFirstUserGestureObserved() {
  web_contents()->SendToAllFrames(
      new AutofillMsg_FirstUserGestureObservedInTab(routing_id()));
}

bool ChromeAutofillClient::IsContextSecure(const GURL& form_origin) {
  content::SSLStatus ssl_status;
  content::NavigationEntry* navigation_entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!navigation_entry)
     return false;

  ssl_status = navigation_entry->GetSSL();
  // Note: If changing the implementation below, also change
  // AwAutofillClient::IsContextSecure. See crbug.com/505388
  return ssl_status.security_style == content::SECURITY_STYLE_AUTHENTICATED &&
         !(ssl_status.content_status &
           content::SSLStatus::RAN_INSECURE_CONTENT);
}

}  // namespace autofill
