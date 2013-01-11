// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_translator.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

// First parameter: Filename of source ONC.
// Second parameter: Filename of expected translated Shill json.
class ONCTranslatorOncToShillTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string> > {
};

// Test the translation from ONC to Shill json.
TEST_P(ONCTranslatorOncToShillTest, Translate) {
  std::string source_onc_filename = GetParam().first;
  scoped_ptr<const base::DictionaryValue> onc_network(
      test_utils::ReadTestDictionary(source_onc_filename));
  std::string result_json_filename = GetParam().second;
  scoped_ptr<const base::DictionaryValue> shill_network(
      test_utils::ReadTestDictionary(result_json_filename));

  scoped_ptr<base::DictionaryValue> translation(TranslateONCObjectToShill(
      &kNetworkConfigurationSignature, *onc_network));

  EXPECT_TRUE(test_utils::Equals(shill_network.get(), translation.get()));
}

// Test different network types, such that each ONC object type is tested at
// least once.
INSTANTIATE_TEST_CASE_P(
    ONCTranslatorOncToShillTest,
    ONCTranslatorOncToShillTest,
    ::testing::Values(
        std::make_pair("managed_ethernet.onc", "shill_ethernet.json"),
        std::make_pair("valid_wifi_psk.onc", "shill_wifi_psk.json"),
        std::make_pair("valid_wifi_clientcert.onc",
                       "shill_wifi_clientcert.json"),
        std::make_pair("valid_wifi_clientref.onc",
                       "shill_wifi_clientref.json"),
        std::make_pair("valid_l2tpipsec.onc", "shill_l2tpipsec.json"),
        std::make_pair("valid_l2tpipsec_clientcert.onc",
                       "shill_l2tpipsec_clientcert.json"),
        std::make_pair("valid_openvpn.onc", "shill_openvpn.json"),
        std::make_pair("valid_openvpn_clientcert.onc",
                       "shill_openvpn_clientcert.json")));

// Test the translation from Shill json to ONC.
//
// Note: This translation direction doesn't have to reconstruct all of the ONC
// fields, as Chrome doesn't need all of a Service's properties.
TEST(ONCTranslatorShillToOncTest, L2TPIPsec) {
  scoped_ptr<base::DictionaryValue> onc_network(
      test_utils::ReadTestDictionary("valid_l2tpipsec.onc"));

  // These two fields are part of the ONC (and are required). However, they
  // don't exist explicitly in the Shill dictionary. As there is no use-case
  // yet, that requires to reconstruct these fields from a Shill dictionary, we
  // don't require their translation.
  onc_network->Remove("VPN.IPsec.AuthenticationType", NULL);
  onc_network->Remove("VPN.IPsec.IKEVersion", NULL);

  scoped_ptr<const base::DictionaryValue> shill_network(
      test_utils::ReadTestDictionary("shill_l2tpipsec.json"));

  scoped_ptr<base::DictionaryValue> translation(TranslateShillServiceToONCPart(
      *shill_network, &kNetworkConfigurationSignature));

  EXPECT_TRUE(test_utils::Equals(onc_network.get(), translation.get()));
}

TEST(ONCTranslatorShillToOncTest, OpenVPN) {
  scoped_ptr<const base::DictionaryValue> onc_network(
      test_utils::ReadTestDictionary("valid_openvpn.onc"));

  scoped_ptr<const base::DictionaryValue> shill_network(
      test_utils::ReadTestDictionary("shill_openvpn.json"));

  scoped_ptr<base::DictionaryValue> translation(TranslateShillServiceToONCPart(
      *shill_network, &kNetworkConfigurationSignature));

  EXPECT_TRUE(test_utils::Equals(onc_network.get(), translation.get()));
}

}  // namespace onc
}  // namespace chromeos
