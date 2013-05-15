// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_MANAGER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service_builder.h"
#include "components/autofill/browser/autofill_manager_delegate.h"

namespace autofill {
class AutofillMetrics;
class AutofillPopupDelegate;
class CreditCard;
class FormStructure;
class PasswordGenerator;
class PersonalDataManager;
struct FormData;
namespace autocheckout {
class WhitelistManager;
}
}

namespace content {
class WebContents;
}

class PersonalDataManager;
class PrefService;

namespace android_webview {

class AwAutofillManagerDelegate :
    public autofill::AutofillManagerDelegate {
 public:
  AwAutofillManagerDelegate(bool enabled);
  virtual ~AwAutofillManagerDelegate();

  void SetSaveFormData(bool enabled);
  bool GetSaveFormData();

  // AutofillManagerDelegate implementation.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual autofill::autocheckout::WhitelistManager*
      GetAutocheckoutWhitelistManager() const OVERRIDE;
  virtual void HideRequestAutocompleteDialog() OVERRIDE;
  virtual void OnAutocheckoutError() OVERRIDE;
  virtual void ShowAutofillSettings() OVERRIDE;
  virtual void ConfirmSaveCreditCard(
      const autofill::AutofillMetrics& metric_logger,
      const autofill::CreditCard& credit_card,
      const base::Closure& save_card_callback) OVERRIDE;
  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounds,
      bool is_google_user,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void HideAutocheckoutBubble() OVERRIDE;
  virtual void ShowRequestAutocompleteDialog(
      const autofill::FormData& form,
      const GURL& source_url,
      autofill::DialogType dialog_type,
      const base::Callback<void(const autofill::FormStructure*,
                                const std::string&)>& callback) OVERRIDE;
  virtual void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      const std::vector<string16>& values,
      const std::vector<string16>& labels,
      const std::vector<string16>& icons,
      const std::vector<int>& identifiers,
      base::WeakPtr<autofill::AutofillPopupDelegate> delegate) OVERRIDE;
  virtual void HideAutofillPopup() OVERRIDE;
  virtual void UpdateProgressBar(double value) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwAutofillManagerDelegate);
};

}  // namespace android_webview

#endif // ANDROID_WEBVIEW_BROWSER_AW_AUTOFILL_MANAGER_DELEGATE_H_
