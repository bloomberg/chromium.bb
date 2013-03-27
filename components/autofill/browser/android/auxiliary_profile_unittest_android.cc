// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/android/auxiliary_profile_loader_android.h"
#include "components/autofill/browser/android/auxiliary_profiles_android.h"
#include "components/autofill/browser/android/test_auxiliary_profile_loader_android.h"
#include "components/autofill/browser/autofill_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class AuxiliaryProfileAndroidTest : public testing::Test {
 public:
  AuxiliaryProfileAndroidTest() {}

  AutofillProfile* GetAndLoadProfile() {
    autofill::AuxiliaryProfilesAndroid impl(profile_loader_);
    profile_ = impl.LoadContactsProfile();
    return profile_.get();
  }

  TestAuxiliaryProfileLoader& profile_loader() {
    return profile_loader_;
  }

 private:
  TestAuxiliaryProfileLoader profile_loader_;
  scoped_ptr<AutofillProfile> profile_;
};

TEST_F(AuxiliaryProfileAndroidTest, SetNameInfo) {
  string16 first_name = ASCIIToUTF16("John");
  string16 middle_name = ASCIIToUTF16("H.");
  string16 last_name = ASCIIToUTF16("Waston");

  profile_loader().SetFirstName(first_name);
  profile_loader().SetMiddleName(middle_name);
  profile_loader().SetLastName(last_name);

  AutofillProfile* profile = GetAndLoadProfile();

  EXPECT_EQ(profile->GetRawInfo(NAME_FIRST), first_name);
  EXPECT_EQ(profile->GetRawInfo(NAME_MIDDLE), middle_name);
  EXPECT_EQ(profile->GetRawInfo(NAME_LAST), last_name);
}

TEST_F(AuxiliaryProfileAndroidTest, SetNameInfoEmpty) {
  AutofillProfile* profile = GetAndLoadProfile();

  EXPECT_EQ(profile->GetRawInfo(NAME_FIRST), string16());
  EXPECT_EQ(profile->GetRawInfo(NAME_MIDDLE), string16());
  EXPECT_EQ(profile->GetRawInfo(NAME_LAST), string16());
}

TEST_F(AuxiliaryProfileAndroidTest, SetEmailInfo) {
  std::vector<string16> email_addresses;
  email_addresses.push_back(ASCIIToUTF16("sherlock@holmes.com"));
  email_addresses.push_back(ASCIIToUTF16("watson@holmes.com"));
  profile_loader().SetEmailAddresses(email_addresses);

  AutofillProfile* profile = GetAndLoadProfile();
  std::vector<string16> loaded_email_addresses;
  profile->GetRawMultiInfo(EMAIL_ADDRESS, &loaded_email_addresses);
  EXPECT_EQ(loaded_email_addresses, email_addresses);
}

TEST_F(AuxiliaryProfileAndroidTest, SetEmailInfoEmpty) {
  std::vector<string16> expected_email_addresses;
  expected_email_addresses.push_back(string16());
  std::vector<string16> loaded_email_addresses;
  AutofillProfile* profile = GetAndLoadProfile();
  profile->GetRawMultiInfo(EMAIL_ADDRESS, &loaded_email_addresses);
  EXPECT_EQ(loaded_email_addresses, expected_email_addresses);
}

TEST_F(AuxiliaryProfileAndroidTest, SetPhoneInfo) {
  std::vector<string16> phone_numbers;
  phone_numbers.push_back(ASCIIToUTF16("6502530000"));
  profile_loader().SetPhoneNumbers(phone_numbers);

  std::vector<string16> loaded_phone_numbers;
  AutofillProfile* profile = GetAndLoadProfile();
  profile->GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &loaded_phone_numbers);
  EXPECT_EQ(loaded_phone_numbers, phone_numbers);
}

TEST_F(AuxiliaryProfileAndroidTest, SetPhoneInfoEmpty) {
  std::vector<string16> expected_phone_numbers;
  expected_phone_numbers.push_back(string16());

  std::vector<string16> loaded_phone_numbers;
  AutofillProfile* profile = GetAndLoadProfile();
  profile->GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &loaded_phone_numbers);
  EXPECT_EQ(loaded_phone_numbers, expected_phone_numbers);
}

//
// Android user's profile contact does not parse its address
// into constituent parts. Instead we just Get a long string blob.
// Disable address population tests until we implement a way to parse the
// data.
//

#if 0
TEST_F(AuxiliaryProfileAndroidTest, SetAddressInfo) {
  string16 street = ASCIIToUTF16("221 B Baker Street");
  string16 city = ASCIIToUTF16("London");
  string16 postal_code = ASCIIToUTF16("123456");
  string16 region = ASCIIToUTF16("Georgian Terrace");
  string16 country = ASCIIToUTF16("England");

  profile_loader().SetStreet(street);
  profile_loader().SetCity(city);
  profile_loader().SetPostalCode(postal_code);
  profile_loader().SetRegion(region);
  profile_loader().SetCountry(country);

  AutofillProfile* profile = GetAndLoadProfile();
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_LINE1), street);
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_CITY), city);
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_ZIP), postal_code);
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_STATE), region);
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_COUNTRY), country);
}

string16 post_office_box= ASCIIToUTF16("222");
string16 neighborhood = ASCIIToUTF16("Doyle");
TEST_F(AuxiliaryProfileAndroidTest, SetAddressInfoCompoundFields1) {
  profile_loader().SetPostOfficeBox(post_office_box);
  profile_loader().SetNeighborhood(neighborhood);
  string16 expectedLine2= ASCIIToUTF16("222, Doyle");
  AutofillProfile* profile = GetAndLoadProfile();
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_LINE2), expectedLine2);
}

TEST_F(AuxiliaryProfileAndroidTest, SetAddressInfoCompoundFields2) {
  profile_loader().SetPostOfficeBox(post_office_box);
  AutofillProfile* profile = GetAndLoadProfile();
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_LINE2), post_office_box);
}

TEST_F(AuxiliaryProfileAndroidTest, SetAddressInfoCompoundFields3) {
  profile_loader().SetNeighborhood(neighborhood);
  AutofillProfile* profile = GetAndLoadProfile();
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_LINE2), neighborhood);
}

TEST_F(AuxiliaryProfileAndroidTest, SetAddressInfoEmpty) {
  AutofillProfile* profile = GetAndLoadProfile();
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_LINE1), string16());
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_LINE2), string16());
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_CITY), string16());
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_ZIP), string16());
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_STATE), string16());
  EXPECT_EQ(profile->GetRawInfo(ADDRESS_HOME_COUNTRY), string16());
}
#endif
