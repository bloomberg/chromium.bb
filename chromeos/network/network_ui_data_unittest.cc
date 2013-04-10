// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_ui_data.h"

#include "base/values.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(NetworkUIDataTest, ONCSource) {
  base::DictionaryValue ui_data_dict;

  ui_data_dict.SetString(NetworkUIData::kKeyONCSource, "user_import");
  {
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(onc::ONC_SOURCE_USER_IMPORT, ui_data.onc_source());
    EXPECT_FALSE(ui_data.is_managed());
  }

  ui_data_dict.SetString(NetworkUIData::kKeyONCSource, "device_policy");
  {
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(onc::ONC_SOURCE_DEVICE_POLICY, ui_data.onc_source());
    EXPECT_TRUE(ui_data.is_managed());
  }
  ui_data_dict.SetString(NetworkUIData::kKeyONCSource, "user_policy");
  {
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(onc::ONC_SOURCE_USER_POLICY, ui_data.onc_source());
    EXPECT_TRUE(ui_data.is_managed());
  }
}

TEST(NetworkUIDataTest, CertificateType) {
  {
    base::DictionaryValue ui_data_dict;
    ui_data_dict.SetString(NetworkUIData::kKeyCertificateType, "none");
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(CLIENT_CERT_TYPE_NONE, ui_data.certificate_type());
  }
  {
    base::DictionaryValue ui_data_dict;
    ui_data_dict.SetString(NetworkUIData::kKeyCertificateType, "ref");
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(CLIENT_CERT_TYPE_REF, ui_data.certificate_type());
  }
  {
    // for type pattern we need to have some kind of pattern
    std::string organization("Little If Any, Inc.");
    base::DictionaryValue ui_data_dict;
    base::DictionaryValue* pattern_dict = new base::DictionaryValue;
    base::DictionaryValue* issuer_dict = new base::DictionaryValue;
    issuer_dict->SetString("Organization", organization);
    pattern_dict->Set("Issuer", issuer_dict);
    ui_data_dict.Set("certificate_pattern", pattern_dict);
    ui_data_dict.SetString(NetworkUIData::kKeyCertificateType, "pattern");
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(CLIENT_CERT_TYPE_PATTERN, ui_data.certificate_type());
  }
}

TEST(NetworkUIDataTest, CertificatePattern) {
  std::string organization("Little If Any, Inc.");
  base::DictionaryValue ui_data_dict;
  base::DictionaryValue* pattern_dict = new base::DictionaryValue;
  base::DictionaryValue* issuer_dict = new base::DictionaryValue;
  issuer_dict->SetString("Organization", organization);
  pattern_dict->Set("Issuer", issuer_dict);
  ui_data_dict.SetString("certificate_type", "pattern");
  ui_data_dict.Set("certificate_pattern", pattern_dict);
  NetworkUIData ui_data(ui_data_dict);
  EXPECT_FALSE(ui_data.certificate_pattern().Empty());
  EXPECT_EQ(organization,
            ui_data.certificate_pattern().issuer().organization());
}

class CreateUIDataTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string> > {
};

TEST_P(CreateUIDataTest, CreateUIDataFromONC) {
  namespace test_utils = onc::test_utils;
  scoped_ptr<base::DictionaryValue> onc_network =
      test_utils::ReadTestDictionary(GetParam().first);

  scoped_ptr<base::DictionaryValue> expected_uidata =
      test_utils::ReadTestDictionary(GetParam().second);

  scoped_ptr<NetworkUIData> actual_uidata =
      CreateUIDataFromONC(onc::ONC_SOURCE_USER_POLICY, *onc_network);
  EXPECT_TRUE(actual_uidata != NULL);

  base::DictionaryValue actual_uidata_dict;
  actual_uidata->FillDictionary(&actual_uidata_dict);
  EXPECT_TRUE(test_utils::Equals(&actual_uidata_dict, expected_uidata.get()));
}

INSTANTIATE_TEST_CASE_P(
    CreateUIDataTest,
    CreateUIDataTest,
    ::testing::Values(
         std::make_pair("valid_wifi_clientcert.onc",
                        "uidata_for_wifi_clientcert.json"),
         std::make_pair("valid_wifi_clientref.onc",
                        "uidata_for_wifi_clientref.json"),
         std::make_pair("valid_wifi_psk.onc",
                        "uidata_for_wifi_psk.json"),
         std::make_pair("valid_openvpn_clientcert.onc",
                        "uidata_for_openvpn_clientcert.json"),
         std::make_pair("valid_l2tpipsec_clientcert.onc",
                        "uidata_for_l2tpipsec_clientcert.json")));

}  // namespace chromeos
