// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/ui/zoom/zoom_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
class WebContents;
}

namespace autofill {

class AutofillDialogController;
class AutofillKeystoneBridgeWrapper;
class AutofillPopupControllerImpl;
class CreditCardScannerController;
struct FormData;

// Chrome implementation of AutofillClient.
class ChromeAutofillClient
    : public AutofillClient,
      public content::WebContentsUserData<ChromeAutofillClient>,
      public content::WebContentsObserver,
      public ui_zoom::ZoomObserver {
 public:
  ~ChromeAutofillClient() override;

  // Called when the tab corresponding to |this| instance is activated.
  void TabActivated();

  // AutofillClient:
  PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  PrefService* GetPrefs() override;
  IdentityProvider* GetIdentityProvider() override;
  rappor::RapporService* GetRapporService() override;
  void HideRequestAutocompleteDialog() override;
  void ShowAutofillSettings() override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(GetRealPanResult result) override;
  void ConfirmSaveCreditCard(const base::Closure& save_card_callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowRequestAutocompleteDialog(
      const FormData& form,
      content::RenderFrameHost* render_frame_host,
      const ResultCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<autofill::Suggestion>& suggestions,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void DetectAccountCreationForms(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  void OnFirstUserGestureObserved() override;
  void LinkClicked(const GURL& url, WindowOpenDisposition disposition) override;

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* rfh) override;
  void DidNavigateAnyFrame(
      content::RenderFrameHost* render_frame_host,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void MainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;

  // ZoomObserver implementation.
  void OnZoomChanged(
      const ui_zoom::ZoomController::ZoomChangedEventData& data) override;

  // Exposed for testing.
  AutofillDialogController* GetDialogControllerForTesting() {
    return dialog_controller_.get();
  }
  void SetDialogControllerForTesting(
      const base::WeakPtr<AutofillDialogController>& dialog_controller) {
    dialog_controller_ = dialog_controller;
  }

 private:
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Creates |bridge_wrapper_|, which is responsible for dealing with Keystone
  // notifications.
  void RegisterForKeystoneNotifications();

  // Deletes |bridge_wrapper_|.
  void UnregisterFromKeystoneNotifications();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  explicit ChromeAutofillClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromeAutofillClient>;

  base::WeakPtr<AutofillDialogController> dialog_controller_;
  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;
  CardUnmaskPromptControllerImpl unmask_controller_;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Listens to Keystone notifications and passes relevant ones on to the
  // PersonalDataManager.
  //
  // The class of this member must remain a forward declaration, even in the
  // .cc implementation file, since the class is defined in a Mac-only
  // implementation file. This means that the pointer cannot be wrapped in a
  // scoped_ptr.
  AutofillKeystoneBridgeWrapper* bridge_wrapper_;
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  // The last render frame that called requestAutocomplete.
  content::RenderFrameHost* last_rfh_to_rac_;

  // The identity provider, used for Wallet integration.
  scoped_ptr<IdentityProvider> identity_provider_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAutofillClient);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
