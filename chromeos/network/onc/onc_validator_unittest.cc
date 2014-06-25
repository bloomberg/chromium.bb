// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

class ONCValidatorTest : public ::testing::Test {
 public:
  // Validate |onc_object| with the given |signature|. The object is considered
  // to be managed if |managed_onc| is true. A strict validator is used if
  // |strict| is true. |onc_object| and the resulting repaired object of the
  // validation is stored, so that expectations can be checked afterwards using
  // one of the Expect* functions below.
  void Validate(bool strict,
                scoped_ptr<base::DictionaryValue> onc_object,
                const OncValueSignature* signature,
                bool managed_onc,
                ::onc::ONCSource onc_source) {
    scoped_ptr<Validator> validator;
    if (strict) {
      // Create a strict validator that complains about every error.
      validator.reset(new Validator(true, true, true, managed_onc));
    } else {
      // Create a liberal validator that ignores or repairs non-critical errors.
      validator.reset(new Validator(false, false, false, managed_onc));
    }
    validator->SetOncSource(onc_source);
    original_object_ = onc_object.Pass();
    repaired_object_ = validator->ValidateAndRepairObject(signature,
                                                          *original_object_,
                                                          &validation_result_);
  }

  void ExpectValid() {
    EXPECT_EQ(Validator::VALID, validation_result_);
    EXPECT_TRUE(test_utils::Equals(original_object_.get(),
                                   repaired_object_.get()));
  }

  void ExpectRepairWithWarnings(
      const base::DictionaryValue& expected_repaired) {
    EXPECT_EQ(Validator::VALID_WITH_WARNINGS, validation_result_);
    EXPECT_TRUE(test_utils::Equals(&expected_repaired, repaired_object_.get()));
  }

  void ExpectInvalid() {
    EXPECT_EQ(Validator::INVALID, validation_result_);
    EXPECT_EQ(NULL, repaired_object_.get());
  }

 private:
  Validator::Result validation_result_;
  scoped_ptr<const base::DictionaryValue> original_object_;
  scoped_ptr<const base::DictionaryValue> repaired_object_;
};

namespace {

struct OncParams {
  // |location_of_object| is a string to identify the object to be tested. It
  // may be used as a filename or as a dictionary key.
  OncParams(const std::string& location_of_object,
            const OncValueSignature* onc_signature,
            bool is_managed_onc,
            ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE)
      : location(location_of_object),
        signature(onc_signature),
        is_managed(is_managed_onc),
        onc_source(onc_source) {
  }

  std::string location;
  const OncValueSignature* signature;
  bool is_managed;
  ::onc::ONCSource onc_source;
};

::std::ostream& operator<<(::std::ostream& os, const OncParams& onc) {
  return os << "(" << onc.location << ", " << onc.signature << ", "
            << (onc.is_managed ? "managed" : "unmanaged") << ", "
            << GetSourceAsString(onc.onc_source) << ")";
}

}  // namespace

// Ensure that the constant |kEmptyUnencryptedConfiguration| describes a valid
// ONC toplevel object.
TEST_F(ONCValidatorTest, EmptyUnencryptedConfiguration) {
  Validate(true, ReadDictionaryFromJson(kEmptyUnencryptedConfiguration),
           &kToplevelConfigurationSignature, false, ::onc::ONC_SOURCE_NONE);
  ExpectValid();
}

// This test case is about validating valid ONC objects without any errors. Both
// the strict and the liberal validator accept the object.
class ONCValidatorValidTest : public ONCValidatorTest,
                              public ::testing::WithParamInterface<OncParams> {
};

TEST_P(ONCValidatorValidTest, StrictValidationValid) {
  OncParams onc = GetParam();
  Validate(true, test_utils::ReadTestDictionary(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  ExpectValid();
}

TEST_P(ONCValidatorValidTest, LiberalValidationValid) {
  OncParams onc = GetParam();
  Validate(false, test_utils::ReadTestDictionary(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  ExpectValid();
}

// The parameters are:
// OncParams(string: Filename of a ONC file that is to be validated,
//           OncValueSignature: signature of that ONC,
//           bool: true if the ONC is managed).
INSTANTIATE_TEST_CASE_P(
    ONCValidatorValidTest,
    ONCValidatorValidTest,
    ::testing::Values(
        OncParams("managed_toplevel1.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("managed_toplevel2.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("managed_toplevel_with_global_config.onc",
                  &kToplevelConfigurationSignature,
                  true),
        // Check that at least one configuration is accepted for
        // device policies.
        OncParams("managed_toplevel_wifi_peap.onc",
                  &kToplevelConfigurationSignature,
                  true,
                  ::onc::ONC_SOURCE_DEVICE_POLICY),
        OncParams("managed_toplevel_l2tpipsec.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("toplevel_wifi_wpa_psk.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_wep_proxy.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_leap.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_eap_clientcert_with_cert_pems.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_remove.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_wifi_open.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_openvpn_clientcert_with_cert_pems.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_empty.onc",
                  &kToplevelConfigurationSignature,
                  false),
        OncParams("toplevel_only_global_config.onc",
                  &kToplevelConfigurationSignature,
                  true),
        OncParams("encrypted.onc", &kToplevelConfigurationSignature, true),
        OncParams("managed_vpn.onc", &kNetworkConfigurationSignature, true),
        OncParams("ethernet.onc", &kNetworkConfigurationSignature, true),
        OncParams("ethernet_with_eap.onc",
                  &kNetworkConfigurationSignature,
                  true),
        OncParams("translation_of_shill_ethernet_with_ipconfig.onc",
                  &kNetworkConfigurationSignature,
                  true),
        OncParams("translation_of_shill_wifi_with_state.onc",
                  &kNetworkWithStateSignature,
                  false),
        OncParams("valid_openvpn_with_cert_pems.onc",
                  &kNetworkConfigurationSignature,
                  false)));

namespace {

struct RepairParams {
  // Both arguments are strings to identify the object that is expected as the
  // validation result. They may either be used as filenames or as dictionary
  // keys.
  RepairParams(std::string strict_repaired,
               std::string liberal_repaired)
      : location_of_strict_repaired(strict_repaired),
        location_of_liberal_repaired(liberal_repaired) {
  }

  std::string location_of_strict_repaired;
  std::string location_of_liberal_repaired;
};

::std::ostream& operator<<(::std::ostream& os, const RepairParams& rp) {
  return os << "(" << rp.location_of_strict_repaired << ", "
            << rp.location_of_liberal_repaired << ")";
}

}  // namespace

// This test case is about validating ONC objects that contain errors which can
// be repaired (then the errors count as warnings). If a location of the
// expected repaired object is given, then it is checked that the validator
// (either strict or liberal) returns this repaired object and the result is
// VALID_WITH_WARNINGS. If the location is the empty string, then it is expected
// that the validator returns NULL and the result INVALID.
class ONCValidatorTestRepairable
    : public ONCValidatorTest,
      public ::testing::WithParamInterface<std::pair<OncParams,
                                                     RepairParams> > {
 public:
  // Load the common test data and return the dictionary at the field with
  // name |name|.
  scoped_ptr<base::DictionaryValue> GetDictionaryFromTestFile(
      const std::string &name) {
    scoped_ptr<const base::DictionaryValue> dict(
        test_utils::ReadTestDictionary("invalid_settings_with_repairs.json"));
    const base::DictionaryValue* onc_object = NULL;
    CHECK(dict->GetDictionary(name, &onc_object));
    return make_scoped_ptr(onc_object->DeepCopy());
  }
};

TEST_P(ONCValidatorTestRepairable, StrictValidation) {
  OncParams onc = GetParam().first;
  Validate(true, GetDictionaryFromTestFile(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  std::string location_of_repaired =
      GetParam().second.location_of_strict_repaired;
  if (location_of_repaired.empty())
    ExpectInvalid();
  else
    ExpectRepairWithWarnings(*GetDictionaryFromTestFile(location_of_repaired));
}

TEST_P(ONCValidatorTestRepairable, LiberalValidation) {
  OncParams onc = GetParam().first;
  Validate(false, GetDictionaryFromTestFile(onc.location), onc.signature,
           onc.is_managed, onc.onc_source);
  std::string location_of_repaired =
      GetParam().second.location_of_liberal_repaired;
  if (location_of_repaired.empty())
    ExpectInvalid();
  else
    ExpectRepairWithWarnings(*GetDictionaryFromTestFile(location_of_repaired));
}

// The parameters for all test case instantations below are:
// OncParams(string: A fieldname in the dictionary from the file
//                   "invalid_settings_with_repairs.json". That nested
//                   dictionary will be tested.
//           OncValueSignature: signature of that ONC,
//           bool: true if the ONC is managed).
// RepairParams(string: A fieldname in the dictionary from the file
//                      "invalid_settings_with_repairs.json". That nested
//                      dictionary is the expected result from strict
//                      validation,
//              string: A fieldname in the dictionary from the file
//                      "invalid_settings_with_repairs.json". That nested
//                      dictionary is the expected result from liberal
//                      validation).

// Strict validator returns INVALID. Liberal validator repairs.
INSTANTIATE_TEST_CASE_P(
    StrictInvalidLiberalRepair,
    ONCValidatorTestRepairable,
    ::testing::Values(
        std::make_pair(OncParams("network-unknown-fieldname",
                                 &kNetworkConfigurationSignature,
                                 false),
                       RepairParams("", "network-repaired")),
        std::make_pair(OncParams("managed-network-unknown-fieldname",
                                 &kNetworkConfigurationSignature,
                                 true),
                       RepairParams("", "managed-network-repaired")),
        std::make_pair(OncParams("managed-network-unknown-recommended",
                                 &kNetworkConfigurationSignature,
                                 true),
                       RepairParams("", "managed-network-repaired")),
        std::make_pair(OncParams("managed-network-dict-recommended",
                                 &kNetworkConfigurationSignature,
                                 true),
                       RepairParams("", "managed-network-repaired")),
        std::make_pair(OncParams("network-missing-required",
                                 &kNetworkConfigurationSignature,
                                 false),
                       RepairParams("", "network-missing-required")),
        std::make_pair(OncParams("managed-network-missing-required",
                                 &kNetworkConfigurationSignature,
                                 true),
                       RepairParams("", "managed-network-missing-required")),
        // Ensure that state values from Shill aren't accepted as
        // configuration.
        std::make_pair(OncParams("network-state-field",
                                 &kNetworkConfigurationSignature,
                                 false),
                       RepairParams("", "network-repaired")),
        std::make_pair(OncParams("network-nested-state-field",
                                 &kNetworkConfigurationSignature,
                                 false),
                       RepairParams("", "network-nested-state-field-repaired")),
        std::make_pair(OncParams("openvpn-missing-verify-x509-name",
                                 &kNetworkConfigurationSignature, false),
                        RepairParams("", "openvpn-missing-verify-x509-name")),
        std::make_pair(OncParams("ipsec-with-client-cert-missing-cacert",
                                 &kIPsecSignature,
                                 false),
                       RepairParams("",
                                    "ipsec-with-client-cert-missing-cacert")),
        std::make_pair(OncParams("toplevel-with-repairable-networks",
                                 &kToplevelConfigurationSignature,
                                 false,
                                 ::onc::ONC_SOURCE_DEVICE_POLICY),
                       RepairParams("", "toplevel-with-repaired-networks"))));

// Strict and liberal validator repair identically.
INSTANTIATE_TEST_CASE_P(
    StrictAndLiberalRepairIdentically,
    ONCValidatorTestRepairable,
    ::testing::Values(
         std::make_pair(OncParams("toplevel-invalid-network",
                                  &kToplevelConfigurationSignature,
                                  false),
                        RepairParams("toplevel-repaired",
                                     "toplevel-repaired")),
         std::make_pair(OncParams("duplicate-network-guid",
                                  &kToplevelConfigurationSignature,
                                  false),
                        RepairParams("repaired-duplicate-network-guid",
                                     "repaired-duplicate-network-guid")),
         std::make_pair(OncParams("duplicate-cert-guid",
                                  &kToplevelConfigurationSignature,
                                  false),
                        RepairParams("repaired-duplicate-cert-guid",
                                     "repaired-duplicate-cert-guid")),
         std::make_pair(OncParams("toplevel-invalid-network",
                                  &kToplevelConfigurationSignature,
                                  true),
                        RepairParams("toplevel-repaired",
                                     "toplevel-repaired")),
         // Ignore recommended arrays in unmanaged ONC.
         std::make_pair(OncParams("network-with-illegal-recommended",
                                  &kNetworkConfigurationSignature,
                                  false),
                        RepairParams("network-repaired", "network-repaired")),
         std::make_pair(OncParams("toplevel-with-vpn",
                                  &kToplevelConfigurationSignature,
                                  false,
                                  ::onc::ONC_SOURCE_DEVICE_POLICY),
                        RepairParams("toplevel-empty", "toplevel-empty")),
         std::make_pair(OncParams("toplevel-with-server-and-ca-cert",
                                  &kToplevelConfigurationSignature,
                                  true,
                                  ::onc::ONC_SOURCE_DEVICE_POLICY),
                        RepairParams("toplevel-server-and-ca-cert-dropped",
                                     "toplevel-server-and-ca-cert-dropped"))));

// Strict and liberal validator both repair, but with different results.
INSTANTIATE_TEST_CASE_P(
    StrictAndLiberalRepairDifferently,
    ONCValidatorTestRepairable,
    ::testing::Values(
         std::make_pair(OncParams("toplevel-with-nested-warning",
                                  &kToplevelConfigurationSignature,
                                  false),
                        RepairParams("toplevel-empty", "toplevel-repaired"))));

// Strict and liberal validator return both INVALID.
INSTANTIATE_TEST_CASE_P(
    StrictAndLiberalInvalid,
    ONCValidatorTestRepairable,
    ::testing::Values(
         std::make_pair(OncParams("network-unknown-value",
                                  &kNetworkConfigurationSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("managed-network-unknown-value",
                                  &kNetworkConfigurationSignature, true),
                        RepairParams("", "")),
         std::make_pair(OncParams("network-value-out-of-range",
                                  &kNetworkConfigurationSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("ipsec-with-psk-and-cacert",
                                  &kIPsecSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("ipsec-with-empty-cacertrefs",
                                  &kIPsecSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("ipsec-with-servercaref-and-servercarefs",
                                  &kIPsecSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("openvpn-with-servercaref-and-servercarefs",
                                  &kOpenVPNSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("eap-with-servercaref-and-servercarefs",
                                  &kEAPSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("managed-network-value-out-of-range",
                                  &kNetworkConfigurationSignature, true),
                        RepairParams("", "")),
         std::make_pair(OncParams("network-wrong-type",
                                  &kNetworkConfigurationSignature, false),
                        RepairParams("", "")),
         std::make_pair(OncParams("managed-network-wrong-type",
                                  &kNetworkConfigurationSignature, true),
                        RepairParams("", "")),
         std::make_pair(OncParams("network-with-client-cert-pattern",
                                  &kNetworkConfigurationSignature, true,
                                  ::onc::ONC_SOURCE_DEVICE_POLICY),
                        RepairParams("", "")),
         std::make_pair(OncParams("openvpn-invalid-verify-x509-type",
                                  &kNetworkConfigurationSignature, false),
                        RepairParams("", ""))
         ));

}  // namespace onc
}  // namespace chromeos
