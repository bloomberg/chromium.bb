// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_validator.h"

#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {

using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::TestdataSource;

using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::STREET_ADDRESS;
using ::i18n::addressinput::RECIPIENT;

// Used to load region rules for this test.
class ValidationTestDataSource : public TestdataSource {
 public:
  ValidationTestDataSource() : TestdataSource(true) {}

  ~ValidationTestDataSource() override {}

  void Get(const std::string& key, const Callback& data_ready) const override {
    data_ready(
        true, key,
        new std::string(
            "{"
            "\"data/CA\": "
            "{\"lang\": \"en\", \"upper\": \"ACNOSZ\", "
            "\"zipex\": \"H3Z 2Y7,V8X 3X4,T0L 1K0,T0H 1A0\", "
            "\"name\": \"CANADA\", "
            "\"fmt\": \"%N%n%O%n%A%n%C %S %Z\", \"id\": \"data/CA\", "
            "\"languages\": \"en\", \"sub_keys\": \"QC\", \"key\": "
            "\"CA\", "
            "\"require\": \"ACSZ\", \"sub_names\": \"Quebec\", "
            "\"sub_zips\": \"G|H|J\"}, "
            "\"data/CA/QC\": "
            "{\"lang\": \"en\", \"key\": \"QC\", "
            "\"id\": \"data/CA/QC\", \"zip\": \"G|H|J\", \"name\": \"Quebec\"}"
            "}"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ValidationTestDataSource);
};

class AutofillProfileValidatorTest : public testing::Test {
 public:
  AutofillProfileValidatorTest()
      : validator_(new AutofillProfileValidator(
            std::unique_ptr<Source>(new ValidationTestDataSource()),
            std::unique_ptr<Storage>(new NullStorage))),
        onvalidated_cb_(
            base::BindOnce(&AutofillProfileValidatorTest::OnValidated,
                           base::Unretained(this))) {}

 protected:
  const std::unique_ptr<AutofillProfileValidator> validator_;

  ~AutofillProfileValidatorTest() override {}

  void OnValidated(AutofillProfile::ValidityState profile_valid) {
    EXPECT_EQ(expected_validity_state_, profile_valid);
  }

  void set_expected_status(AutofillProfile::ValidityState profile_valid) {
    expected_validity_state_ = profile_valid;
  }

  bool AreRulesLoadedForRegion(std::string region_code) {
    return validator_->AreRulesLoadedForRegion(region_code);
  }

  void LoadRulesForRegion(std::string region_code) {
    validator_->LoadRulesForRegion(region_code);
  }

  AutofillProfileValidatorCallback onvalidated_cb_;

 private:
  AutofillProfile::ValidityState expected_validity_state_;

  base::test::ScopedTaskEnvironment scoped_task_scheduler;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileValidatorTest);
};

// Validate a Null profile.
TEST_F(AutofillProfileValidatorTest, ValidateNullProfile) {
  set_expected_status(AutofillProfile::UNVALIDATED);
  validator_->ValidateProfile(nullptr, std::move(onvalidated_cb_));
}
// Validate a valid profile, for which the rules are not loaded, yet.
TEST_F(AutofillProfileValidatorTest, ValidateFullValidProfile_RulesNotLoaded) {
  // This is a valid profile, and the rules are loaded in the constructors
  // Province: "QC", Country: "CA"
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  set_expected_status(AutofillProfile::VALID);

  std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(false, AreRulesLoadedForRegion(country_code));

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

// Validate a Full Profile, for which the rules are already loaded.
TEST_F(AutofillProfileValidatorTest, ValidateAddress_RulesLoaded) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  set_expected_status(AutofillProfile::VALID);

  std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  LoadRulesForRegion(country_code);
  EXPECT_EQ(true, AreRulesLoadedForRegion(country_code));

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

// When country code is invalid, the profile is invalid.
TEST_F(AutofillProfileValidatorTest, ValidateProfile_CountryCodeNotExists) {
  const std::string country_code = "PP";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  set_expected_status(AutofillProfile::INVALID);

  EXPECT_EQ(false, AreRulesLoadedForRegion(country_code));

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

// When country code is valid, but the rule is not in the source, the profile
// is unvalidated.
TEST_F(AutofillProfileValidatorTest, ValidateAddress_RuleNotExists) {
  const std::string country_code = "US";
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16(country_code));
  set_expected_status(AutofillProfile::UNVALIDATED);

  EXPECT_EQ(false, AreRulesLoadedForRegion(country_code));

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid phone number and a valid address.
TEST_F(AutofillProfileValidatorTest,
       ValidateProfile_InvalidPhone_ValidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());

  set_expected_status(AutofillProfile::INVALID);

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with a valid phone number and an invalid address.
TEST_F(AutofillProfileValidatorTest,
       ValidateProfile_ValidPhone_InvalidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  // QQ is an invalid admin area, thus an invalid address.
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("QQ"));

  set_expected_status(AutofillProfile::INVALID);

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

// Validate a profile with an invalid phone number and an invalid address.
TEST_F(AutofillProfileValidatorTest,
       ValidateProfile_InvalidPhone_InvalidAddress) {
  AutofillProfile profile(autofill::test::GetFullValidProfile());
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, base::string16());
  // QQ is an invalid admin area, thus an invalid address.
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::UTF8ToUTF16("QQ"));

  set_expected_status(AutofillProfile::INVALID);

  validator_->ValidateProfile(&profile, std::move(onvalidated_cb_));
}

}  // namespace autofill
