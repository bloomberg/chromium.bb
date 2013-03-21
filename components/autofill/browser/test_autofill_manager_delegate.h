// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_TEST_AUTOFILL_MANAGER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_BROWSER_TEST_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "components/autofill/browser/autofill_manager_delegate.h"

namespace autofill {

// This class is only for easier writing of testings. All pure virtual functions
// have been giving empty methods.
class TestAutofillManagerDelegate : public AutofillManagerDelegate {
 public:
  TestAutofillManagerDelegate();
  virtual ~TestAutofillManagerDelegate();

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
      autofill::PasswordGenerator* generator) OVERRIDE;
  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounding_box,
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillManagerDelegate);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_TEST_AUTOFILL_MANAGER_DELEGATE_H_
