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
  AddProfile(imported_profile);
  return imported_profile.guid();
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

std::vector<CreditCard*> TestPersonalDataManager::GetCreditCards() const {
  // TODO(crbug.com/778436): The real PersonalDataManager relies on its
  // |pref_service_| to decide what to return. Since the lack of a pref_service_
  // makes this fake class crash, it might be useful to refactor the real
  // GetCreditCards()'s logic into overrideable methods and then remove this
  // function.
  std::vector<CreditCard*> result;
  result.reserve(local_credit_cards_.size() + server_credit_cards_.size());
  for (const auto& card : local_credit_cards_)
    result.push_back(card.get());
  for (const auto& card : server_credit_cards_)
    result.push_back(card.get());
  return result;
}

const std::string& TestPersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    return PersonalDataManager::GetDefaultCountryCodeForNewAddress();

  return default_country_code_;
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
