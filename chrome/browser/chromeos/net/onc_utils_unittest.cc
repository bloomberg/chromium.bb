// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/onc_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

TEST(ONCUtils, ProxySettingsToProxyConfig) {
  scoped_ptr<base::Value> test_data =
      google_apis::test_util::LoadJSONFile("chromeos/net/proxy_config.json");

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

}  // namespace onc
}  // namespace chromeos
