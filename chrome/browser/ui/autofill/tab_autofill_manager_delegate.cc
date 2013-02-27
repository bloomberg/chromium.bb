// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
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

InfoBarService* TabAutofillManagerDelegate::GetInfoBarService() {
  return InfoBarService::FromWebContents(web_contents_);
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

ProfileSyncServiceBase* TabAutofillManagerDelegate::GetProfileSyncService() {
  return ProfileSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

bool TabAutofillManagerDelegate::IsSavingPasswordsEnabled() const {
  return PasswordManager::FromWebContents(web_contents_)->IsSavingEnabled();
}

void TabAutofillManagerDelegate::OnAutocheckoutError() {
  // TODO(ahutter): Notify |dialog_controller_| of the error once it stays open
  // for Autocheckout.
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
    const base::Closure& callback) {
  autofill::ShowAutocheckoutBubble(bounding_box, native_view, callback);
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
      web_contents()->GetView()->GetContentNativeView(),
      element_bounds_in_screen_space);

  popup_controller_->Show(values, labels, icons, identifiers);
}

void TabAutofillManagerDelegate::HideAutofillPopup() {
  if (popup_controller_)
    popup_controller_->Hide();
}

void TabAutofillManagerDelegate::UpdateProgressBar(double value) {
  // TODO(ahutter): Notify |dialog_controller_| of the change once it stays open
  // for Autocheckout.
}

void TabAutofillManagerDelegate::HideRequestAutocompleteDialog() {
  if (dialog_controller_) {
    dialog_controller_->Hide();
    RequestAutocompleteDialogClosed();
  }
}

void TabAutofillManagerDelegate::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (dialog_controller_ &&
      dialog_controller_->dialog_type() ==
          autofill::DIALOG_TYPE_REQUEST_AUTOCOMPLETE) {
    HideRequestAutocompleteDialog();
  }
}

}  // namespace autofill
