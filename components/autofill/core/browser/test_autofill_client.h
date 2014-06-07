// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

// This class is for easier writing of tests.
class TestAutofillClient : public AutofillClient {
 public:
  TestAutofillClient();
  virtual ~TestAutofillClient();

  // AutofillClient implementation.
  virtual PersonalDataManager* GetPersonalDataManager() OVERRIDE;
  virtual scoped_refptr<AutofillWebDataService> GetDatabase() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual void HideRequestAutocompleteDialog() OVERRIDE;
  virtual void ShowAutofillSettings() OVERRIDE;
  virtual void ConfirmSaveCreditCard(
      const AutofillMetrics& metric_logger,
      const base::Closure& save_card_callback) OVERRIDE;
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const ResultCallback& callback) OVERRIDE;
  virtual void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels,
      const std::vector<base::string16>& icons,
      const std::vector<int>& identifiers,
      base::WeakPtr<AutofillPopupDelegate> delegate) OVERRIDE;
  virtual void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) OVERRIDE;
  virtual void HideAutofillPopup() OVERRIDE;
  virtual bool IsAutocompleteEnabled() OVERRIDE;

  virtual void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms) OVERRIDE;

  virtual void DidFillOrPreviewField(
      const base::string16& autofilled_value,
      const base::string16& profile_full_name) OVERRIDE;

  void SetPrefs(scoped_ptr<PrefService> prefs) { prefs_ = prefs.Pass(); }

 private:
  // NULL by default.
  scoped_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillClient);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
