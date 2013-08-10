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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble.h"
#include "chrome/browser/ui/autofill/autocheckout_bubble_controller.h"
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
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
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

autocheckout::WhitelistManager*
TabAutofillManagerDelegate::GetAutocheckoutWhitelistManager() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return autocheckout::WhitelistManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

void TabAutofillManagerDelegate::OnAutocheckoutError() {
  // |dialog_controller_| is a WeakPtr, but we require it to be present when
  // |OnAutocheckoutError| is called, so we intentionally do not do NULL check.
  dialog_controller_->OnAutocheckoutError();
}

void TabAutofillManagerDelegate::OnAutocheckoutSuccess() {
  dialog_controller_->OnAutocheckoutSuccess();
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

bool TabAutofillManagerDelegate::ShowAutocheckoutBubble(
    const gfx::RectF& bounding_box,
    bool is_google_user,
    const base::Callback<void(AutocheckoutBubbleState)>& callback) {
#if !defined(TOOLKIT_VIEWS)
  callback.Run(AUTOCHECKOUT_BUBBLE_CANCELED);
  NOTIMPLEMENTED();
  return false;
#else
  HideAutocheckoutBubble();

  // Convert |bounding_box| to be in screen space.
  gfx::Rect container_rect;
  web_contents_->GetView()->GetContainerBounds(&container_rect);
  gfx::RectF anchor = bounding_box + container_rect.OffsetFromOrigin();

  autocheckout_bubble_ =
      AutocheckoutBubble::Create(scoped_ptr<AutocheckoutBubbleController>(
          new AutocheckoutBubbleController(
              anchor,
              web_contents_->GetView()->GetTopLevelNativeWindow(),
              is_google_user,
              callback)));

  if (!autocheckout_bubble_)
    return false;

  autocheckout_bubble_->ShowBubble();
  return true;
#endif  // #if !defined(TOOLKIT_VIEWS)
}

void TabAutofillManagerDelegate::HideAutocheckoutBubble() {
  if (autocheckout_bubble_.get())
    autocheckout_bubble_->HideBubble();
}

void TabAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback) {
  HideRequestAutocompleteDialog();

  dialog_controller_ = AutofillDialogController::Create(web_contents_,
                                                        form,
                                                        source_url,
                                                        dialog_type,
                                                        callback);
  if (dialog_controller_) {
    dialog_controller_->Show();
  } else {
    callback.Run(NULL, std::string());
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

void TabAutofillManagerDelegate::AddAutocheckoutStep(
    AutocheckoutStepType step_type) {
  dialog_controller_->AddAutocheckoutStep(step_type);
}

void TabAutofillManagerDelegate::UpdateAutocheckoutStep(
    AutocheckoutStepType step_type,
    AutocheckoutStepStatus step_status) {
  dialog_controller_->UpdateAutocheckoutStep(step_type, step_status);
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

  HideAutocheckoutBubble();

  if (!dialog_controller_.get())
    return;

  // A redirect immediately after a successful Autocheckout flow shouldn't hide
  // the dialog.
  bool preserve_dialog = AutofillDriverImpl::FromWebContents(web_contents())->
      autofill_manager()->autocheckout_manager()->should_preserve_dialog();
  bool was_redirect = details.entry &&
      content::PageTransitionIsRedirect(details.entry->GetTransitionType());

  if (dialog_controller_->GetDialogType() == DIALOG_TYPE_REQUEST_AUTOCOMPLETE ||
      (!was_redirect && !preserve_dialog)) {
    HideRequestAutocompleteDialog();
  }
}

void TabAutofillManagerDelegate::WebContentsDestroyed(
    content::WebContents* web_contents) {
  HideAutofillPopup();
}

}  // namespace autofill
