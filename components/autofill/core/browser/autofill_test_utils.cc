// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"

#include <string>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/common/signin_pref_names.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace test {

std::unique_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  return PrefServiceForTesting(registry.get());
}

std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry) {
  AutofillManager::RegisterProfilePrefs(registry);

  // PDM depends on these prefs, which are normally registered in
  // SigninManagerFactory.
  registry->RegisterStringPref(::prefs::kGoogleServicesAccountId,
                               std::string());
  registry->RegisterStringPref(::prefs::kGoogleServicesLastAccountId,
                               std::string());
  registry->RegisterStringPref(::prefs::kGoogleServicesLastUsername,
                               std::string());
  registry->RegisterStringPref(::prefs::kGoogleServicesUserAccountId,
                               std::string());
  registry->RegisterStringPref(::prefs::kGoogleServicesUsername,
                               std::string());

  // PDM depends on these prefs, which are normally registered in
  // AccountTrackerServiceFactory.
  registry->RegisterListPref(AccountTrackerService::kAccountInfoPref);
  registry->RegisterIntegerPref(::prefs::kAccountIdMigrationState,
                                AccountTrackerService::MIGRATION_NOT_STARTED);
  registry->RegisterInt64Pref(AccountFetcherService::kLastUpdatePref, 0);

  PrefServiceFactory factory;
  factory.set_user_prefs(make_scoped_refptr(new TestingPrefStore()));
  return factory.Create(registry);
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field) {
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
  field->is_focusable = true;
}

void CreateTestSelectField(const char* label,
                           const char* name,
                           const char* value,
                           const std::vector<const char*>& values,
                           const std::vector<const char*>& contents,
                           size_t select_size,
                           FormFieldData* field) {
  // Fill the base attributes.
  CreateTestFormField(label, name, value, "select-one", field);

  std::vector<base::string16> values16(select_size);
  for (size_t i = 0; i < select_size; ++i)
    values16[i] = base::UTF8ToUTF16(values[i]);

  std::vector<base::string16> contents16(select_size);
  for (size_t i = 0; i < select_size; ++i)
    contents16[i] = base::UTF8ToUTF16(contents[i]);

  field->option_values = values16;
  field->option_contents = contents16;
}

void CreateTestSelectField(const std::vector<const char*>& values,
                           FormFieldData* field) {
  CreateTestSelectField("", "", "", values, values, values.size(), field);
}

void CreateTestAddressFormData(FormData* form) {
  std::vector<ServerFieldTypeSet> types;
  CreateTestAddressFormData(form, &types);
}

void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types) {
  form->name = ASCIIToUTF16("MyForm");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  types->clear();

  FormFieldData field;
  ServerFieldTypeSet type_set;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_FIRST);
  types->push_back(type_set);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_MIDDLE);
  types->push_back(type_set);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(NAME_LAST);
  types->push_back(type_set);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_LINE1);
  types->push_back(type_set);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_LINE2);
  types->push_back(type_set);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_CITY);
  types->push_back(type_set);
  test::CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_STATE);
  types->push_back(type_set);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_ZIP);
  types->push_back(type_set);
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(ADDRESS_HOME_COUNTRY);
  types->push_back(type_set);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(PHONE_HOME_WHOLE_NUMBER);
  types->push_back(type_set);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
  type_set.clear();
  type_set.insert(EMAIL_ADDRESS);
  types->push_back(type_set);
}

inline void check_and_set(
    FormGroup* profile, ServerFieldType type, const char* value) {
  if (value)
    profile->SetRawInfo(type, base::UTF8ToUTF16(value));
}

AutofillProfile GetFullValidProfile() {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  SetProfileInfo(&profile, "Alice", "", "Wonderland", "alice@wonderland.ca",
                 "Fiction", "666 Notre-Dame Ouest", "Apt 8", "Montreal", "QC",
                 "H3B 2T9", "CA", "15141112233");
  return profile;
}

AutofillProfile GetFullProfile() {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  SetProfileInfo(&profile,
                 "John",
                 "H.",
                 "Doe",
                 "johndoe@hades.com",
                 "Underworld",
                 "666 Erebus St.",
                 "Apt 8",
                 "Elysium", "CA",
                 "91111",
                 "US",
                 "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2() {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  SetProfileInfo(&profile,
                 "Jane",
                 "A.",
                 "Smith",
                 "jsmith@example.com",
                 "ACME",
                 "123 Main Street",
                 "Unit 1",
                 "Greensdale", "MI",
                 "48838",
                 "US",
                 "13105557889");
  return profile;
}

AutofillProfile GetIncompleteProfile1() {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  SetProfileInfo(&profile, "John", "H.", "Doe", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "");
  return profile;
}

AutofillProfile GetIncompleteProfile2() {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  SetProfileInfo(&profile, "", "", "", "jsmith@example.com", "", "", "", "", "",
                 "", "", "");
  return profile;
}

AutofillProfile GetVerifiedProfile() {
  AutofillProfile profile(GetFullProfile());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

AutofillProfile GetVerifiedProfile2() {
  AutofillProfile profile(GetFullProfile2());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::GenerateGUID(), "http://www.example.com");
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    "11", "2022", "1");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  SetCreditCardInfo(&credit_card, "Someone Else", "378282246310005" /* AmEx */,
                    "07", "2022", "1");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetMaskedServerCard() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2020", "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  return credit_card;
}

CreditCard GetMaskedServerCardAmex() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&credit_card, "Justin Thyme", "8431" /* Amex */, "9",
                          "2020", "1");
  credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  return credit_card;
}

void SetProfileInfo(AutofillProfile* profile,
    const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  check_and_set(profile, NAME_FIRST, first_name);
  check_and_set(profile, NAME_MIDDLE, middle_name);
  check_and_set(profile, NAME_LAST, last_name);
  check_and_set(profile, EMAIL_ADDRESS, email);
  check_and_set(profile, COMPANY_NAME, company);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2);
  check_and_set(profile, ADDRESS_HOME_CITY, city);
  check_and_set(profile, ADDRESS_HOME_STATE, state);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone);
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
    const char* guid, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email,
                 company, address1, address2, city, state, zipcode, country,
                 phone);
}

void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id) {
  check_and_set(credit_card, CREDIT_CARD_NAME_FULL, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
  credit_card->set_billing_address_id(billing_address_id);
}

void DisableSystemServices(PrefService* prefs) {
  // Use a mock Keychain rather than the OS one to store credit card data.
  OSCryptMocker::SetUp();
}

void ReenableSystemServices() {
  OSCryptMocker::TearDown();
}

void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards) {
  std::vector<CreditCard> as_masked_cards = cards;
  for (CreditCard& card : as_masked_cards) {
    card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    card.SetNumber(card.LastFourDigits());
    card.SetNetworkForMaskedCard(card.network());
  }
  table->SetServerCreditCards(as_masked_cards);

  for (const CreditCard& card : cards) {
    if (card.record_type() != CreditCard::FULL_SERVER_CARD)
      continue;

    table->UnmaskServerCreditCard(card, card.number());
  }
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type) {
  field->set_signature(signature);
  if (name)
    field->set_name(name);
  if (control_type)
    field->set_type(control_type);
  if (autocomplete)
    field->set_autocomplete(autocomplete);
  field->set_autofill_type(autofill_type);
}

void FillQueryField(AutofillQueryContents::Form::Field* field,
                    unsigned signature,
                    const char* name,
                    const char* control_type) {
  field->set_signature(signature);
  if (name)
    field->set_name(name);
  if (control_type)
    field->set_type(control_type);
}

}  // namespace test
}  // namespace autofill
