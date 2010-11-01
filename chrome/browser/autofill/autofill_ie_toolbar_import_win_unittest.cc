// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_ie_toolbar_import_win.h"

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/win/registry.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/sync/util/data_encryption.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

// Defined in autofill_ie_toolbar_import_win.cc. Not exposed in the header file.
bool ImportCurrentUserProfiles(std::vector<AutoFillProfile>* profiles,
                               std::vector<CreditCard>* credit_cards);

namespace {

const wchar_t kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const wchar_t kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";

const wchar_t* const kProfileKey =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Profiles";
const wchar_t* const kCreditCardKey =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Credit Cards";
const wchar_t* const kPasswordHashValue = L"password_hash";
const wchar_t* const kSaltValue = L"salt";

struct ValueDescription {
  wchar_t const* const value_name;
  wchar_t const* const value;
};

ValueDescription profile1[] = {
  { L"name_first", L"John" },
  { L"name_middle", L"Herman" },
  { L"name_last", L"Doe" },
  { L"email", L"jdoe@test.com" },
  { L"company_name", L"Testcompany" },
  { L"phone_home_number", L"555-5555" },
  { L"phone_home_city_code", L"444" },
  { L"phone_home_country_code", L"1" },
};

ValueDescription profile2[] = {
  { L"name_first", L"Jane" },
  { L"name_last", L"Doe" },
  { L"email", L"janedoe@test.com" },
  { L"company_name", L"Testcompany" },
  { L"phone_fax_number", L"555-6666" },
  { L"phone_fax_city_code", L"777" },
  { L"phone_fax_country_code", L"2" },
};

ValueDescription credit_card[] = {
  { L"credit_card_name", L"Tommy Gun" },
  // "12345790087665675" encrypted:
  { L"credit_card_number", L"\x18B7\xE586\x459B\x7457\xA066\x3842\x71DA"
                           L"\x4854\xB906\x9C7C\x50A6\x4376\xFD9D\x1E02"
                           L"\x790A\x7330\xB77B\xAF32\x93EB\xB84F\xEC8F"
                           L"\x265B\xD0E1\x4E27\xB758\x7985\xB92F" },
  { L"credit_card_exp_month", L"11" },
  { L"credit_card_exp_4_digit_year", L"2011" },
};

ValueDescription empty_salt = {
  L"salt", L"\x1\x2\x3\x4\x5\x6\x7\x8\x9\xA\xB\xC\xD\xE\xF\x10\x11\x12\x13\x14"
};

ValueDescription empty_password = {
  L"password_hash", L""
};

ValueDescription protected_salt = {
  L"salt", L"\x4854\xB906\x9C7C\x50A6\x4376\xFD9D\x1E02"
};

ValueDescription protected_password = {
  L"password_hash", L"\x18B7\xE586\x459B\x7457\xA066\x3842\x71DA"
};

void EncryptAndWrite(RegKey* key, const ValueDescription* value) {
  string data;
  size_t data_size = (lstrlen(value->value) + 1) * sizeof(wchar_t);
  data.resize(data_size);
  memcpy(&data[0], value->value, data_size);

  std::vector<uint8> encrypted_data = EncryptData(data);
  EXPECT_TRUE(key->WriteValue(value->value_name, &encrypted_data[0],
                              encrypted_data.size(), REG_BINARY));
}

void CreateSubkey(RegKey* key, wchar_t const* subkey_name,
                  const ValueDescription* values, size_t values_size) {
  RegKey subkey;
  subkey.Create(key->Handle(), subkey_name, KEY_ALL_ACCESS);
  EXPECT_TRUE(subkey.Valid());
  for (size_t i = 0; i < values_size; ++i)
    EncryptAndWrite(&subkey, values + i);
}

}  // namespace

class AutofillIeToolbarImportTest : public testing::Test {
 public:
  AutofillIeToolbarImportTest();

  // testing::Test method overrides:
  virtual void SetUp();
  virtual void TearDown();

 private:
  RegKey temp_hkcu_hive_key_;

  DISALLOW_COPY_AND_ASSIGN(AutofillIeToolbarImportTest);
};

AutofillIeToolbarImportTest::AutofillIeToolbarImportTest() {
}

void AutofillIeToolbarImportTest::SetUp() {
  temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER,
                             kUnitTestUserOverrideSubKey,
                             KEY_ALL_ACCESS);
  EXPECT_TRUE(temp_hkcu_hive_key_.Valid());
  EXPECT_EQ(ERROR_SUCCESS, RegOverridePredefKey(HKEY_CURRENT_USER,
                                                temp_hkcu_hive_key_.Handle()));
}

void AutofillIeToolbarImportTest::TearDown() {
  EXPECT_EQ(ERROR_SUCCESS, RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
  temp_hkcu_hive_key_.Close();
  RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey, KEY_ALL_ACCESS);
  key.DeleteKey(L"");
}

TEST_F(AutofillIeToolbarImportTest, TestAutoFillImport) {
  RegKey profile_key;
  profile_key.Create(HKEY_CURRENT_USER, kProfileKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(profile_key.Valid());

  CreateSubkey(&profile_key, L"0", profile1, arraysize(profile1));
  CreateSubkey(&profile_key, L"1", profile2, arraysize(profile2));

  RegKey cc_key;
  cc_key.Create(HKEY_CURRENT_USER, kCreditCardKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(cc_key.Valid());
  CreateSubkey(&cc_key, L"0", credit_card, arraysize(credit_card));
  EncryptAndWrite(&cc_key, &empty_password);
  EncryptAndWrite(&cc_key, &empty_salt);

  profile_key.Close();
  cc_key.Close();

  std::vector<AutoFillProfile> profiles;
  std::vector<CreditCard> credit_cards;
  EXPECT_TRUE(ImportCurrentUserProfiles(&profiles, &credit_cards));
  ASSERT_EQ(profiles.size(), 2);
  // The profiles are read in reverse order.
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(NAME_FIRST)),
            profile1[0].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(NAME_MIDDLE)),
            profile1[1].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(NAME_LAST)),
            profile1[2].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(EMAIL_ADDRESS)),
            profile1[3].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(COMPANY_NAME)),
            profile1[4].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(PHONE_HOME_COUNTRY_CODE)),
            profile1[7].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(PHONE_HOME_CITY_CODE)),
            profile1[6].value);
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(PHONE_HOME_NUMBER)),
            L"5555555");
  EXPECT_EQ(profiles[1].GetFieldText(AutoFillType(PHONE_HOME_WHOLE_NUMBER)),
            L"14445555555");

  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(NAME_FIRST)),
            profile2[0].value);
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(NAME_LAST)),
            profile2[1].value);
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(EMAIL_ADDRESS)),
            profile2[2].value);
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(COMPANY_NAME)),
            profile2[3].value);
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(PHONE_FAX_COUNTRY_CODE)),
            profile2[6].value);
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(PHONE_FAX_CITY_CODE)),
            profile2[5].value);
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(PHONE_FAX_NUMBER)),
            L"5556666");
  EXPECT_EQ(profiles[0].GetFieldText(AutoFillType(PHONE_FAX_WHOLE_NUMBER)),
            L"27775556666");

  ASSERT_EQ(credit_cards.size(), 1);
  EXPECT_EQ(credit_cards[0].GetFieldText(AutoFillType(CREDIT_CARD_NAME)),
            credit_card[0].value);
  EXPECT_EQ(credit_cards[0].GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)),
            L"12345790087665675");
  EXPECT_EQ(credit_cards[0].GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)),
            credit_card[2].value);
  EXPECT_EQ(credit_cards[0].GetFieldText(
            AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)),
            credit_card[3].value);

  // Mock password encrypted cc.
  cc_key.Open(HKEY_CURRENT_USER, kCreditCardKey, KEY_ALL_ACCESS);
  EXPECT_TRUE(cc_key.Valid());
  EncryptAndWrite(&cc_key, &protected_password);
  EncryptAndWrite(&cc_key, &protected_salt);
  cc_key.Close();

  profiles.clear();
  credit_cards.clear();
  EXPECT_TRUE(ImportCurrentUserProfiles(&profiles, &credit_cards));
  // Profiles are not protected.
  EXPECT_EQ(profiles.size(), 2);
  // Credit cards are.
  EXPECT_EQ(credit_cards.size(), 0);
}

