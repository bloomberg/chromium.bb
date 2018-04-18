// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_personal_data_manager.h"

#include "components/autofill/core/browser/personal_data_manager_observer.h"

namespace autofill {

TestPersonalDataManager::TestPersonalDataManager()
    : PersonalDataManager("en-US") {}

TestPersonalDataManager::~TestPersonalDataManager() {}

void TestPersonalDataManager::RecordUseOf(const AutofillDataModel& data_model) {
  CreditCard* credit_card = GetCreditCardWithGUID(data_model.guid().c_str());
  if (credit_card)
    credit_card->RecordAndLogUse();

  AutofillProfile* profile = GetProfileWithGUID(data_model.guid().c_str());
  if (profile)
    profile->RecordAndLogUse();
}

std::string TestPersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  num_times_save_imported_profile_called_++;
  return PersonalDataManager::SaveImportedProfile(imported_profile);
}

std::string TestPersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_credit_card) {
  AddCreditCard(imported_credit_card);
  return imported_credit_card.guid();
}

void TestPersonalDataManager::AddProfile(const AutofillProfile& profile) {
  std::unique_ptr<AutofillProfile> profile_ptr =
      std::make_unique<AutofillProfile>(profile);
  web_profiles_.push_back(std::move(profile_ptr));
  NotifyPersonalDataChanged();
}

void TestPersonalDataManager::RemoveByGUID(const std::string& guid) {
  CreditCard* credit_card = GetCreditCardWithGUID(guid.c_str());
  if (credit_card) {
    local_credit_cards_.erase(
        std::find_if(local_credit_cards_.begin(), local_credit_cards_.end(),
                     [credit_card](const std::unique_ptr<CreditCard>& ptr) {
                       return ptr.get() == credit_card;
                     }));
  }

  AutofillProfile* profile = GetProfileWithGUID(guid.c_str());
  if (profile) {
    web_profiles_.erase(
        std::find_if(web_profiles_.begin(), web_profiles_.end(),
                     [profile](const std::unique_ptr<AutofillProfile>& ptr) {
                       return ptr.get() == profile;
                     }));
  }
}

void TestPersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> local_credit_card =
      std::make_unique<CreditCard>(credit_card);
  local_credit_cards_.push_back(std::move(local_credit_card));
  NotifyPersonalDataChanged();
}

void TestPersonalDataManager::AddFullServerCreditCard(
    const CreditCard& credit_card) {
  // Though the name is AddFullServerCreditCard, this test class treats masked
  // and full server cards equally, relying on their preset RecordType to
  // differentiate them.
  AddServerCreditCard(credit_card);
}

std::vector<AutofillProfile*> TestPersonalDataManager::GetProfiles() const {
  std::vector<AutofillProfile*> result;
  result.reserve(web_profiles_.size());
  for (const auto& profile : web_profiles_)
    result.push_back(profile.get());
  return result;
}

const std::string& TestPersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    return PersonalDataManager::GetDefaultCountryCodeForNewAddress();

  return default_country_code_;
}

void TestPersonalDataManager::SetProfiles(
    std::vector<AutofillProfile>* profiles) {
  // Copy all the profiles. Called by functions like
  // PersonalDataManager::SaveImportedProfile, which impact metrics.
  ClearProfiles();
  for (const auto& profile : *profiles)
    AddProfile(profile);
}

void TestPersonalDataManager::LoadProfiles() {
  // Overridden to avoid a trip to the database. This should be a no-op except
  // for the side-effect of logging the profile count.
  pending_profiles_query_ = 123;
  pending_server_profiles_query_ = 124;
  {
    std::vector<std::unique_ptr<AutofillProfile>> profiles;
    web_profiles_.swap(profiles);
    std::unique_ptr<WDTypedResult> result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
        AUTOFILL_PROFILES_RESULT, std::move(profiles));
    OnWebDataServiceRequestDone(pending_profiles_query_, std::move(result));
  }
  {
    std::vector<std::unique_ptr<AutofillProfile>> profiles;
    server_profiles_.swap(profiles);
    std::unique_ptr<WDTypedResult> result = std::make_unique<
        WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
        AUTOFILL_PROFILES_RESULT, std::move(profiles));
    OnWebDataServiceRequestDone(pending_server_profiles_query_,
                                std::move(result));
  }
}

void TestPersonalDataManager::LoadCreditCards() {
  // Overridden to avoid a trip to the database.
  pending_creditcards_query_ = 125;
  pending_server_creditcards_query_ = 126;
  {
    std::vector<std::unique_ptr<CreditCard>> credit_cards;
    local_credit_cards_.swap(credit_cards);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
            AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
    OnWebDataServiceRequestDone(pending_creditcards_query_, std::move(result));
  }
  {
    std::vector<std::unique_ptr<CreditCard>> credit_cards;
    server_credit_cards_.swap(credit_cards);
    std::unique_ptr<WDTypedResult> result =
        std::make_unique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
            AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
    OnWebDataServiceRequestDone(pending_server_creditcards_query_,
                                std::move(result));
  }
}

bool TestPersonalDataManager::IsAutofillEnabled() const {
  // Return the value of autofill_enabled_ if it has been set, otherwise fall
  // back to the normal behavior of checking the pref_service.
  if (autofill_enabled_.has_value())
    return autofill_enabled_.value();
  return PersonalDataManager::IsAutofillEnabled();
}

bool TestPersonalDataManager::IsAutofillCreditCardEnabled() const {
  // Return the value of autofill_credit_card_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_credit_card_enabled_.has_value())
    return autofill_credit_card_enabled_.value();
  return PersonalDataManager::IsAutofillCreditCardEnabled();
}

bool TestPersonalDataManager::IsAutofillWalletImportEnabled() const {
  // Return the value of autofill_wallet_import_enabled_ if it has been set,
  // otherwise fall back to the normal behavior of checking the pref_service.
  if (autofill_wallet_import_enabled_.has_value())
    return autofill_wallet_import_enabled_.value();
  return PersonalDataManager::IsAutofillWalletImportEnabled();
}

std::string TestPersonalDataManager::CountryCodeForCurrentTimezone()
    const {
  return timezone_country_code_;
}

void TestPersonalDataManager::ClearProfiles() {
  web_profiles_.clear();
}

void TestPersonalDataManager::ClearCreditCards() {
  local_credit_cards_.clear();
  server_credit_cards_.clear();
}

AutofillProfile* TestPersonalDataManager::GetProfileWithGUID(const char* guid) {
  for (AutofillProfile* profile : GetProfiles()) {
    if (!profile->guid().compare(guid))
      return profile;
  }
  return nullptr;
}

CreditCard* TestPersonalDataManager::GetCreditCardWithGUID(const char* guid) {
  for (CreditCard* card : GetCreditCards()) {
    if (!card->guid().compare(guid))
      return card;
  }
  return nullptr;
}

void TestPersonalDataManager::AddServerCreditCard(
    const CreditCard& credit_card) {
  std::unique_ptr<CreditCard> server_credit_card =
      std::make_unique<CreditCard>(credit_card);
  server_credit_cards_.push_back(std::move(server_credit_card));
  NotifyPersonalDataChanged();
}

}  // namespace autofill
