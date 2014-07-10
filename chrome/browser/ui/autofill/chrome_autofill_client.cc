// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "ui/gfx/rect.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::ChromeAutofillClient);

namespace autofill {

ChromeAutofillClient::ChromeAutofillClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), web_contents_(web_contents) {
  DCHECK(web_contents);
  // Since ZoomController is also a WebContentsObserver, we need to be careful
  // about disconnecting from it since the relative order of destruction of
  // WebContentsObservers is not guaranteed. ZoomController silently clears
  // its ZoomObserver list during WebContentsDestroyed() so there's no need
  // to explicitly remove ourselves on destruction.
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  // There may not always be a ZoomController, e.g. on Android.
  if (zoom_controller)
    zoom_controller->AddObserver(this);
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
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

scoped_refptr<AutofillWebDataService> ChromeAutofillClient::GetDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      profile, Profile::EXPLICIT_ACCESS);
}

PrefService* ChromeAutofillClient::GetPrefs() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->GetPrefs();
}

void ChromeAutofillClient::ShowAutofillSettings() {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser)
    chrome::ShowSettingsSubPage(browser, chrome::kAutofillSubPage);
#endif  // #if defined(OS_ANDROID)
}

void ChromeAutofillClient::ConfirmSaveCreditCard(
    const AutofillMetrics& metric_logger,
    const base::Closure& save_card_callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  AutofillCCInfoBarDelegate::Create(
      infobar_service, &metric_logger, save_card_callback);
}

void ChromeAutofillClient::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const ResultCallback& callback) {
  HideRequestAutocompleteDialog();

  dialog_controller_ = AutofillDialogController::Create(
      web_contents_, form, source_url, callback);
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
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels,
    const std::vector<base::string16>& icons,
    const std::vector<int>& identifiers,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  // Convert element_bounds to be in screen space.
  gfx::Rect client_area = web_contents_->GetContainerBounds();
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

  popup_controller_->Show(values, labels, icons, identifiers);
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
      ChromePasswordManagerClient::FromWebContents(web_contents_);
  if (password_client)
    password_client->HidePasswordGenerationPopup();
}

bool ChromeAutofillClient::IsAutocompleteEnabled() {
  // For browser, Autocomplete is always enabled as part of Autofill.
  return GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void ChromeAutofillClient::HideRequestAutocompleteDialog() {
  if (dialog_controller_.get())
    dialog_controller_->Hide();
}

void ChromeAutofillClient::WebContentsDestroyed() {
  HideAutofillPopup();
}

void ChromeAutofillClient::OnZoomChanged(
    const ZoomController::ZoomChangedEventData& data) {
  HideAutofillPopup();
}

void ChromeAutofillClient::DetectAccountCreationForms(
    const std::vector<autofill::FormStructure*>& forms) {
  password_manager::PasswordGenerationManager* manager =
      ChromePasswordManagerClient::GetGenerationManagerFromWebContents(
          web_contents_);
  if (manager)
    manager->DetectAccountCreationForms(forms);
}

void ChromeAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
#if defined(OS_ANDROID)
  AutofillLoggerAndroid::DidFillOrPreviewField(autofilled_value,
                                               profile_full_name);
#endif  // defined(OS_ANDROID)
}

}  // namespace autofill
