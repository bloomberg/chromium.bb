// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/autocheckout_whitelist_manager_factory.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/browser/password_generator.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/password_form.h"
#include "ui/gfx/rect.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::TabAutofillManagerDelegate);

namespace autofill {

TabAutofillManagerDelegate::TabAutofillManagerDelegate(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      dialog_controller_(NULL) {
  DCHECK(web_contents);
}

TabAutofillManagerDelegate::~TabAutofillManagerDelegate() {
  HideAutofillPopup();
}

PersonalDataManager* TabAutofillManagerDelegate::GetPersonalDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

PrefService* TabAutofillManagerDelegate::GetPrefs() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())->
      GetPrefs();
}

autocheckout::WhitelistManager*
TabAutofillManagerDelegate::GetAutocheckoutWhitelistManager() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return autocheckout::WhitelistManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

bool TabAutofillManagerDelegate::IsSavingPasswordsEnabled() const {
  return PasswordManager::FromWebContents(web_contents_)->IsSavingEnabled();
}

bool TabAutofillManagerDelegate::IsPasswordSyncEnabled() const {
  ProfileSyncServiceBase* service = ProfileSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!service)
    return false;

  syncer::ModelTypeSet sync_set = service->GetPreferredDataTypes();
  return service->HasSyncSetupCompleted() && sync_set.Has(syncer::PASSWORDS);
}

void TabAutofillManagerDelegate::SetSyncStateChangedCallback(
    const base::Closure& callback) {
  ProfileSyncServiceBase* service = ProfileSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  if (!service)
    return;

  if (sync_state_changed_callback_.is_null() && !callback.is_null())
    service->AddObserver(this);
  else if (!sync_state_changed_callback_.is_null() && callback.is_null())
    service->RemoveObserver(this);

  sync_state_changed_callback_ = callback;

  // Invariant: Either sync_state_changed_callback_.is_null() is true
  // and this object is not subscribed as a
  // ProfileSyncServiceObserver, or
  // sync_state_changed_callback_.is_null() is false and this object
  // is subscribed as a ProfileSyncServiceObserver.
}

void TabAutofillManagerDelegate::OnAutocheckoutError() {
  dialog_controller_->OnAutocheckoutError();
}

void TabAutofillManagerDelegate::ShowAutofillSettings() {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser)
    chrome::ShowSettingsSubPage(browser, chrome::kAutofillSubPage);
#endif  // #if defined(OS_ANDROID)
}

void TabAutofillManagerDelegate::ConfirmSaveCreditCard(
    const AutofillMetrics& metric_logger,
    const CreditCard& credit_card,
    const base::Closure& save_card_callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  AutofillCCInfoBarDelegate::Create(
      infobar_service, &metric_logger, save_card_callback);
}

void TabAutofillManagerDelegate::ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  browser->window()->ShowPasswordGenerationBubble(bounds, form, generator);
#endif  // #if defined(OS_ANDROID)
}

void TabAutofillManagerDelegate::ShowAutocheckoutBubble(
    const gfx::RectF& bounding_box,
    const gfx::NativeView& native_view,
    const base::Callback<void(bool)>& callback) {
  HideAutocheckoutBubble();
  autocheckout_bubble_ =
      AutocheckoutBubble::Create(scoped_ptr<AutocheckoutBubbleController>(
          new AutocheckoutBubbleController(bounding_box,
                                           native_view,
                                           callback)));
  autocheckout_bubble_->ShowBubble();
}

void TabAutofillManagerDelegate::HideAutocheckoutBubble() {
  if (autocheckout_bubble_)
    autocheckout_bubble_->HideBubble();
}

void TabAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const content::SSLStatus& ssl_status,
    const AutofillMetrics& metric_logger,
    DialogType dialog_type,
    const base::Callback<void(const FormStructure*)>& callback) {
  HideRequestAutocompleteDialog();

  dialog_controller_ =
      new autofill::AutofillDialogControllerImpl(web_contents_,
                                                 form,
                                                 source_url,
                                                 ssl_status,
                                                 metric_logger,
                                                 dialog_type,
                                                 callback);
  dialog_controller_->Show();
}

void TabAutofillManagerDelegate::RequestAutocompleteDialogClosed() {
  dialog_controller_ = NULL;
}

void TabAutofillManagerDelegate::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& identifiers,
    AutofillPopupDelegate* delegate) {
  // Convert element_bounds to be in screen space.
  gfx::Rect client_area;
  web_contents_->GetView()->GetContainerBounds(&client_area);
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();

  // Will delete or reuse the old |popup_controller_|.
  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_,
      delegate,
      web_contents()->GetView()->GetNativeView(),
      element_bounds_in_screen_space);

  popup_controller_->Show(values, labels, icons, identifiers);
}

void TabAutofillManagerDelegate::HideAutofillPopup() {
  if (popup_controller_)
    popup_controller_->Hide();
}

void TabAutofillManagerDelegate::UpdateProgressBar(double value) {
  dialog_controller_->UpdateProgressBar(value);
}

void TabAutofillManagerDelegate::HideRequestAutocompleteDialog() {
  if (dialog_controller_) {
    dialog_controller_->Hide();
    RequestAutocompleteDialogClosed();
  }
}

void TabAutofillManagerDelegate::OnStateChanged() {
  if (!sync_state_changed_callback_.is_null())
    sync_state_changed_callback_.Run();
}

void TabAutofillManagerDelegate::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (dialog_controller_ &&
      dialog_controller_->dialog_type() ==
          autofill::DIALOG_TYPE_REQUEST_AUTOCOMPLETE) {
    HideRequestAutocompleteDialog();
  }

  HideAutocheckoutBubble();
}

}  // namespace autofill
