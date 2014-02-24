// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_

#include <vector>

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"

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

  virtual const std::vector<AutofillProfile*>& GetProfiles() const OVERRIDE;
  virtual const std::vector<AutofillProfile*>& web_profiles() const OVERRIDE;
  virtual const std::vector<CreditCard*>& GetCreditCards() const OVERRIDE;

  virtual std::string SaveImportedProfile(
      const AutofillProfile& imported_profile) OVERRIDE;
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card) OVERRIDE;

  virtual std::string CountryCodeForCurrentTimezone() const OVERRIDE;
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const
      OVERRIDE;

  void set_timezone_country_code(const std::string& timezone_country_code) {
    timezone_country_code_ = timezone_country_code;
  }
  void set_default_country_code(const std::string& default_country_code) {
    default_country_code_ = default_country_code;
  }

  const AutofillProfile& imported_profile() { return imported_profile_; }
  const CreditCard& imported_credit_card() { return imported_credit_card_; }

 private:
  std::vector<AutofillProfile*> profiles_;
  std::vector<CreditCard*> credit_cards_;
  AutofillProfile imported_profile_;
  CreditCard imported_credit_card_;
  std::string timezone_country_code_;
  std::string default_country_code_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_PERSONAL_DATA_MANAGER_H_
