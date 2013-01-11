// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/onc_utils.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

TEST(ONCUtils, ProxySettingsToProxyConfig) {
  scoped_ptr<base::Value> test_data =
      google_apis::test_util::LoadJSONFile("net/proxy_config.json");

  base::ListValue* list_of_tests;
  test_data->GetAsList(&list_of_tests);

  int index = 0;
  for (base::ListValue::iterator it = list_of_tests->begin();
       it != list_of_tests->end(); ++it, ++index) {
    SCOPED_TRACE("Test case #" + base::IntToString(index));

    base::DictionaryValue* test_case;
    (*it)->GetAsDictionary(&test_case);

    base::DictionaryValue* onc_proxy_settings;
    test_case->GetDictionary("ONC_ProxySettings", &onc_proxy_settings);

    base::DictionaryValue* expected_proxy_config;
    test_case->GetDictionary("ProxyConfig", &expected_proxy_config);

    scoped_ptr<base::DictionaryValue> actual_proxy_config =
        ConvertOncProxySettingsToProxyConfig(*onc_proxy_settings);
    EXPECT_TRUE(test_utils::Equals(expected_proxy_config,
                                   actual_proxy_config.get()));
  }
}

class ONCCreateUIDataTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string> > {
};

TEST_P(ONCCreateUIDataTest, CreateUIData) {
  scoped_ptr<base::DictionaryValue> onc_network =
      test_utils::ReadTestDictionary(GetParam().first);

  scoped_ptr<base::Value> expected_uidata =
      google_apis::test_util::LoadJSONFile(GetParam().second);
  const base::DictionaryValue* expected_uidata_dict;
  expected_uidata->GetAsDictionary(&expected_uidata_dict);

  scoped_ptr<NetworkUIData> actual_uidata =
      CreateUIData(ONC_SOURCE_USER_POLICY, *onc_network);
  EXPECT_TRUE(actual_uidata != NULL);

  base::DictionaryValue actual_uidata_dict;
  actual_uidata->FillDictionary(&actual_uidata_dict);
  EXPECT_TRUE(test_utils::Equals(&actual_uidata_dict, expected_uidata_dict));
}

INSTANTIATE_TEST_CASE_P(
    ONCCreateUIDataTest,
    ONCCreateUIDataTest,
    ::testing::Values(
         std::make_pair("valid_wifi_clientcert.onc",
                        "net/uidata_for_wifi_clientcert.json"),
         std::make_pair("valid_wifi_clientref.onc",
                        "net/uidata_for_wifi_clientref.json"),
         std::make_pair("valid_wifi_psk.onc", "net/uidata_for_wifi_psk.json"),
         std::make_pair("valid_openvpn_clientcert.onc",
                        "net/uidata_for_openvpn_clientcert.json"),
         std::make_pair("valid_l2tpipsec_clientcert.onc",
                        "net/uidata_for_l2tpipsec_clientcert.json")));

}  // namespace onc
}  // namespace chromeos
