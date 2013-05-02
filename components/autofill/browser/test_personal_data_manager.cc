// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/test_personal_data_manager.h"

#include "components/autofill/browser/personal_data_manager_observer.h"

namespace autofill {

TestPersonalDataManager::TestPersonalDataManager()
    : PersonalDataManager("en-US") {}

TestPersonalDataManager::~TestPersonalDataManager() {}

void TestPersonalDataManager::AddTestingProfile(AutofillProfile* profile) {
  profiles_.push_back(profile);
  FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                    OnPersonalDataChanged());
}

void TestPersonalDataManager::AddTestingCreditCard(CreditCard* credit_card) {
  credit_cards_.push_back(credit_card);
  FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                    OnPersonalDataChanged());
}

const std::vector<AutofillProfile*>& TestPersonalDataManager::GetProfiles() {
  return profiles_;
}

const std::vector<CreditCard*>& TestPersonalDataManager::
    GetCreditCards() const {
  return credit_cards_;
}

void TestPersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  imported_profile_ = imported_profile;
}

}  // namespace autofill
