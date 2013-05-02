// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_AUTOFILL_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_BROWSER_AUTOFILL_TEST_PERSONAL_DATA_MANAGER_H_

#include <vector>

#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/personal_data_manager.h"

namespace autofill {

// A simplistic PersonalDataManager used for testing.
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager();
  virtual ~TestPersonalDataManager();

  // Adds |profile| to |profiles_|. This does not take ownership of |profile|.
  void AddTestingProfile(AutofillProfile* profile);

  // Adds |credit_card| to |credit_cards_|. This does not take ownership of
  // |credit_card|.
  void AddTestingCreditCard(CreditCard* credit_card);

  virtual const std::vector<AutofillProfile*>& GetProfiles() OVERRIDE;
  virtual void SaveImportedProfile(const AutofillProfile& imported_profile)
      OVERRIDE;

  const AutofillProfile& imported_profile() { return imported_profile_; }
  virtual const std::vector<CreditCard*>& GetCreditCards() const OVERRIDE;

 private:
  std::vector<AutofillProfile*> profiles_;
  std::vector<CreditCard*> credit_cards_;
  AutofillProfile imported_profile_;
};

}  // namespace autofill

#endif  // COMPONENTS_BROWSER_AUTOFILL_TEST_PERSONAL_DATA_MANAGER_H_
