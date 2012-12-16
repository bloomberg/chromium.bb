// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <string>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {
namespace {

// Create a strict validator that complains about every error.
scoped_ptr<Validator> CreateStrictValidator(bool managed_onc) {
  return make_scoped_ptr(new Validator(true, true, true, managed_onc));
}

// Create a liberal validator that ignores or repairs non-critical errors.
scoped_ptr<Validator> CreateLiberalValidator(bool managed_onc) {
  return make_scoped_ptr(new Validator(false, false, false, managed_onc));
}
}  // namespace

TEST(ONCValidatorValidTest, EmptyUnencryptedConfiguration) {
  scoped_ptr<Validator> validator(CreateStrictValidator(true));
  scoped_ptr<const base::DictionaryValue> original(
      ReadDictionaryFromJson(kEmptyUnencryptedConfiguration));

  Validator::Result result;
  scoped_ptr<const base::DictionaryValue> repaired(
      validator->ValidateAndRepairObject(&kToplevelConfigurationSignature,
                                         *original, &result));
  EXPECT_EQ(Validator::VALID, result);
  EXPECT_TRUE(test_utils::Equals(original.get(), repaired.get()));
}

// This test case is about validating valid ONC objects.
class ONCValidatorValidTest
    : public ::testing::TestWithParam<
        std::pair<std::string, const OncValueSignature*> > {
 protected:
  std::string GetFilename() const {
    return GetParam().first;
  }

  const OncValueSignature* GetSignature() const {
    return GetParam().second;
  }
};

TEST_P(ONCValidatorValidTest, ValidPolicyOnc) {
  scoped_ptr<Validator> validator(CreateStrictValidator(true));
  scoped_ptr<const base::DictionaryValue> original(
      test_utils::ReadTestDictionary(GetFilename()));

  Validator::Result result;
  scoped_ptr<const base::DictionaryValue> repaired(
      validator->ValidateAndRepairObject(GetSignature(), *original, &result));
  EXPECT_EQ(Validator::VALID, result);
  EXPECT_TRUE(test_utils::Equals(original.get(), repaired.get()));
}

INSTANTIATE_TEST_CASE_P(
    ONCValidatorValidTest,
    ONCValidatorValidTest,
    ::testing::Values(std::make_pair("managed_toplevel.onc",
                                     &kToplevelConfigurationSignature),
                      std::make_pair("encrypted.onc",
                                     &kToplevelConfigurationSignature),
                      std::make_pair("managed_vpn.onc",
                                     &kNetworkConfigurationSignature),
                      std::make_pair("managed_ethernet.onc",
                                     &kNetworkConfigurationSignature),
                      // Ignore recommended arrays in unmanaged ONC:
                      std::make_pair("recommended_in_unmanaged.onc",
                                     &kNetworkConfigurationSignature)));

// Validate invalid ONC objects and check the resulting repaired object. This
// test fixture loads a test json file into |invalid_| containing several test
// objects which can be accessed by their path. The test boolean parameter
// determines wether to use the strict or the liberal validator.
class ONCValidatorInvalidTest : public ::testing::TestWithParam<bool> {
 public:
  // Validate the entry at |path_to_object| with the given
  // |signature|. Depending on |managed_onc| the object is interpreted as a
  // managed onc (with recommended fields) or not. The resulting repaired object
  // must match the entry at |path_to_repaired| if the liberal validator is
  // used.
  void ValidateInvalid(const std::string& path_to_object,
                       const std::string& path_to_repaired,
                       const OncValueSignature* signature,
                       bool managed_onc) {
    scoped_ptr<Validator> validator;
    if (GetParam())
      validator = CreateStrictValidator(managed_onc);
    else
      validator = CreateLiberalValidator(managed_onc);

    const base::DictionaryValue* object = NULL;
    ASSERT_TRUE(invalid_->GetDictionary(path_to_object, &object));

    Validator::Result result;
    scoped_ptr<base::DictionaryValue> actual_repaired =
        validator->ValidateAndRepairObject(signature, *object, &result);
    if (GetParam() || path_to_repaired == "") {
      EXPECT_EQ(Validator::INVALID, result);
      EXPECT_EQ(NULL, actual_repaired.get());
    } else {
      EXPECT_EQ(Validator::VALID_WITH_WARNINGS, result);
      const base::DictionaryValue* expected_repaired = NULL;
      invalid_->GetDictionary(path_to_repaired, &expected_repaired);
      EXPECT_TRUE(test_utils::Equals(expected_repaired, actual_repaired.get()));
    }
  }

  virtual void SetUp() {
    invalid_ =
        test_utils::ReadTestDictionary("invalid_settings_with_repairs.json");
  }

  scoped_ptr<const base::DictionaryValue> invalid_;
};

TEST_P(ONCValidatorInvalidTest, UnknownFieldName) {
  ValidateInvalid("network-unknown-fieldname",
                  "network-repaired",
                  &kNetworkConfigurationSignature, false);
  ValidateInvalid("managed-network-unknown-fieldname",
                  "managed-network-repaired",
                  &kNetworkConfigurationSignature, true);
}

TEST_P(ONCValidatorInvalidTest, UnknownType) {
  ValidateInvalid("network-unknown-type",
                  "",
                  &kNetworkConfigurationSignature, false);
  ValidateInvalid("managed-network-unknown-type",
                  "",
                  &kNetworkConfigurationSignature, true);
}

TEST_P(ONCValidatorInvalidTest, UnknownRecommendedFieldName) {
  ValidateInvalid("managed-network-unknown-recommended",
                  "managed-network-repaired",
                  &kNetworkConfigurationSignature, true);
}

TEST_P(ONCValidatorInvalidTest, DictionaryRecommended) {
  ValidateInvalid("managed-network-dict-recommended",
                  "managed-network-repaired",
                  &kNetworkConfigurationSignature, true);
}

TEST_P(ONCValidatorInvalidTest, MissingRequiredField) {
  ValidateInvalid("network-missing-required",
                  "network-missing-required",
                  &kNetworkConfigurationSignature, false);
  ValidateInvalid("managed-network-missing-required",
                  "managed-network-missing-required",
                  &kNetworkConfigurationSignature, true);
}

TEST_P(ONCValidatorInvalidTest, RecommendedInUnmanaged) {
  ValidateInvalid("network-illegal-recommended",
                  "network-repaired",
                  &kNetworkConfigurationSignature, false);
}

INSTANTIATE_TEST_CASE_P(ONCValidatorInvalidTest,
                        ONCValidatorInvalidTest,
                        ::testing::Bool());

}  // namespace onc
}  // namespace chromeos
