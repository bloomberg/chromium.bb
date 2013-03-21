// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class AutofillPopupControllerImpl;

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
class WebContents;
}

namespace autofill {

class AutocheckoutBubble;
class AutofillDialogControllerImpl;

// Chrome implementation of AutofillManagerDelegate.
class TabAutofillManagerDelegate
    : public AutofillManagerDelegate,
      public ProfileSyncServiceObserver,
      public content::WebContentsUserData<TabAutofillManagerDelegate>,
      public content::WebContentsObserver {
 public:
  virtual ~TabAutofillManagerDelegate();

  // AutofillManagerDelegate implementation.
  virtual PersonalDataManager* GetPersonalDataManager() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual autocheckout::WhitelistManager*
      GetAutocheckoutWhitelistManager() const OVERRIDE;
  virtual void HideRequestAutocompleteDialog() OVERRIDE;
  virtual bool IsSavingPasswordsEnabled() const OVERRIDE;
  virtual bool IsPasswordSyncEnabled() const OVERRIDE;
  virtual void SetSyncStateChangedCallback(
      const base::Closure& callback) OVERRIDE;
  virtual void OnAutocheckoutError() OVERRIDE;
  virtual void ShowAutofillSettings() OVERRIDE;
  virtual void ConfirmSaveCreditCard(
      const AutofillMetrics& metric_logger,
      const CreditCard& credit_card,
      const base::Closure& save_card_callback) OVERRIDE;
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      PasswordGenerator* generator) OVERRIDE;
  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounds,
      const gfx::NativeView& native_view,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void HideAutocheckoutBubble() OVERRIDE;
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const AutofillMetrics& metric_logger,
      DialogType dialog_type,
      const base::Callback<void(const FormStructure*)>& callback) OVERRIDE;
  virtual void RequestAutocompleteDialogClosed() OVERRIDE;
  virtual void ShowAutofillPopup(const gfx::RectF& element_bounds,
                                 const std::vector<string16>& values,
                                 const std::vector<string16>& labels,
                                 const std::vector<string16>& icons,
                                 const std::vector<int>& identifiers,
                                 AutofillPopupDelegate* delegate) OVERRIDE;
  virtual void HideAutofillPopup() OVERRIDE;
  virtual void UpdateProgressBar(double value) OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  explicit TabAutofillManagerDelegate(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabAutofillManagerDelegate>;

  base::Closure sync_state_changed_callback_;
  content::WebContents* const web_contents_;
  AutofillDialogControllerImpl* dialog_controller_;  // weak.
  base::WeakPtr<AutocheckoutBubble> autocheckout_bubble_;
  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;

  DISALLOW_COPY_AND_ASSIGN(TabAutofillManagerDelegate);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_
