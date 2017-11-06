// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_validation_util.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {

using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::TestdataSource;

class AutofillAddressValidationTest : public testing::Test,
                                      public LoadRulesListener {
 public:
  AutofillAddressValidationTest() {
    base::FilePath file_path;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
    file_path = file_path.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("libaddressinput"))
                    .Append(FILE_PATH_LITERAL("src"))
                    .Append(FILE_PATH_LITERAL("testdata"))
                    .Append(FILE_PATH_LITERAL("countryinfo.txt"));

    validator_ = std::make_unique<AddressValidator>(
        std::unique_ptr<Source>(
            new TestdataSource(true, file_path.AsUTF8Unsafe())),
        std::unique_ptr<Storage>(new NullStorage), this);
    validator_->LoadRules("CA");
  }

  AutofillProfile::ValidityState ValidateAddressTest(AutofillProfile* profile) {
    return address_validation_util::ValidateAddress(profile, validator_.get());
  }

  ~AutofillAddressValidationTest() override {}

 private:
  std::unique_ptr<AddressValidator> validator_;

  // LoadRulesListener implementation.
  void OnAddressValidationRulesLoaded(const std::string& country_code,
                                      bool success) override {}

  DISALLOW_COPY_AND_ASSIGN(AutofillAddressValidationTest);
};

TEST_F(AutofillAddressValidationTest, ValidateNULLProfile) {
  EXPECT_EQ(AutofillProfile::UNVALIDATED, ValidateAddressTest(nullptr));
}

TEST_F(AutofillAddressValidationTest, ValidateFullValidProfile) {
  // This is a valid profile according to the rules in ValidationTestDataSource:
  // Address Line 1: "666 Notre-Dame Ouest",
  // Address Line 2: "Apt 8", City: "Montreal", Province: "QC",
  // Postal Code: "H3B 2T9", Country Code: "CA",
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  EXPECT_EQ(AutofillProfile::VALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateFullProfile_CountryCodeNotExist) {
  // This is a profile with invalid country code, therefore it cannot be
  // validated according to ValidationTestDataSource.
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateFullProfile_EmptyCountryCode) {
  // This is a profile with no country code, therefore it cannot be validated
  // according to ValidationTestDataSource.
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(""));
  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateFullProfile_RuleNotLoaded) {
  // This is a profile with valid country code, but the rule is not loaded.
  const std::string country_code = "US";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  EXPECT_EQ(AutofillProfile::UNVALIDATED, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::UNVALIDATED,
            profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_AdminAreaNotExists) {
  const std::string admin_area_code = "QQ";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area_code));

  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_EmptyAdminArea) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(""));

  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_AdminAreaFullName) {
  const std::string admin_area = "Quebec";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));

  EXPECT_EQ(AutofillProfile::VALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_AdminAreaSmallCode) {
  const std::string admin_area = "qc";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));

  EXPECT_EQ(AutofillProfile::VALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_AdminAreaSpecialLetter) {
  const std::string admin_area = "Québec";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(admin_area));

  EXPECT_EQ(AutofillProfile::VALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_ValidZipNoSpace) {
  // TODO(crbug/752614): postal codes in lower case letters should also be
  // considered as valid. Now, they are considered as INVALID.
  const std::string postal_code = "H3C6S3";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(postal_code));

  EXPECT_EQ(AutofillProfile::VALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::VALID, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_InvalidZip) {
  const std::string postal_code = "ABC 123";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(postal_code));

  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::INVALID,
            profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest, ValidateAddress_EmptyZip) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(""));

  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::VALID,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::EMPTY, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

TEST_F(AutofillAddressValidationTest,
       ValidateFullProfile_EmptyCountryCodeAreaAndZip) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(""));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16(""));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, base::UTF8ToUTF16(""));
  EXPECT_EQ(AutofillProfile::INVALID, ValidateAddressTest(&profile));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(AutofillProfile::EMPTY,
            profile.GetValidityState(ADDRESS_HOME_STATE));
  EXPECT_EQ(AutofillProfile::EMPTY, profile.GetValidityState(ADDRESS_HOME_ZIP));
}

// TODO(crbug/754727): add tests for a non-default language.
// Ex: Nouveau-Brunswick for Canada.

// TODO(crbug/754729): Add tests for a country whose default language is a
// non-Western one, such as China.

}  // namespace autofill
