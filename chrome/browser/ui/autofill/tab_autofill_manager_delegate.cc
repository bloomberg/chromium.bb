// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/password_generation_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/core/common/autofill_messages.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/rect.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::TabAutofillManagerDelegate);

namespace autofill {

TabAutofillManagerDelegate::TabAutofillManagerDelegate(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents) {
  DCHECK(web_contents);
}

TabAutofillManagerDelegate::~TabAutofillManagerDelegate() {
  // NOTE: It is too late to clean up the autofill popup; that cleanup process
  // requires that the WebContents instance still be valid and it is not at
  // this point (in particular, the WebContentsImpl destructor has already
  // finished running and we are now in the base class destructor).
  DCHECK(!popup_controller_);
}

void TabAutofillManagerDelegate::TabActivated(int reason) {
  if (reason != TabStripModelObserver::CHANGE_REASON_USER_GESTURE)
    return;

  if (dialog_controller_.get())
    dialog_controller_->TabActivated();
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
    const base::Closure& save_card_callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  AutofillCCInfoBarDelegate::Create(
      infobar_service, &metric_logger, save_card_callback);
}

void TabAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const base::Callback<void(const FormStructure*)>& callback) {
  HideRequestAutocompleteDialog();

  dialog_controller_ = AutofillDialogController::Create(web_contents_,
                                                        form,
                                                        source_url,
                                                        callback);
  if (dialog_controller_) {
    dialog_controller_->Show();
  } else {
    callback.Run(NULL);
    NOTIMPLEMENTED();
  }
}

void TabAutofillManagerDelegate::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& identifiers,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  // Convert element_bounds to be in screen space.
  gfx::Rect client_area;
  web_contents_->GetView()->GetContainerBounds(&client_area);
  gfx::RectF element_bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();

  // Will delete or reuse the old |popup_controller_|.
  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_,
      delegate,
      web_contents(),
      web_contents()->GetView()->GetNativeView(),
      element_bounds_in_screen_space,
      text_direction);

  popup_controller_->Show(values, labels, icons, identifiers);
}

void TabAutofillManagerDelegate::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  if (popup_controller_.get())
    popup_controller_->UpdateDataListValues(values, labels);
}

void TabAutofillManagerDelegate::HideAutofillPopup() {
  if (popup_controller_.get())
    popup_controller_->Hide();
}

bool TabAutofillManagerDelegate::IsAutocompleteEnabled() {
  // For browser, Autocomplete is always enabled as part of Autofill.
  return GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void TabAutofillManagerDelegate::HideRequestAutocompleteDialog() {
  if (dialog_controller_.get())
    dialog_controller_->Hide();
}

void TabAutofillManagerDelegate::WasShown() {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new AutofillMsg_PageShown(host->GetRoutingID()));
}

void TabAutofillManagerDelegate::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!dialog_controller_.get())
    return;

  HideRequestAutocompleteDialog();
}

void TabAutofillManagerDelegate::WebContentsDestroyed(
    content::WebContents* web_contents) {
  HideAutofillPopup();
}

void TabAutofillManagerDelegate::DetectAccountCreationForms(
    const std::vector<autofill::FormStructure*>& forms) {
  PasswordGenerationManager* manager =
      PasswordGenerationManager::FromWebContents(web_contents_);
  if (manager)
    manager->DetectAccountCreationForms(forms);
}

}  // namespace autofill
