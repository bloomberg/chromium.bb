// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// A mock network for testing. We really only need the ui_data() member.
class TestNetwork : public Network {
 public:
  TestNetwork()
      : Network("wifi-network", TYPE_WIFI) {}
};

}  // namespace

class NetworkUIDataTest : public testing::Test {
 protected:
  NetworkUIDataTest() {}

  static void SetProperty(DictionaryValue* dict,
                          const char* property_key,
                          const char* controller,
                          base::Value* default_value) {
    DictionaryValue* property_dict = new DictionaryValue();
    if (controller) {
      property_dict->SetString(NetworkPropertyUIData::kKeyController,
                               controller);
    }
    if (default_value) {
      property_dict->Set(NetworkPropertyUIData::kKeyDefaultValue,
                         default_value);
    }
    dict->Set(base::StringPrintf("%s.%s",
                                 NetworkUIData::kKeyProperties,
                                 property_key),
              property_dict);
  }

  static void CheckProperty(const DictionaryValue* dict,
                            const char* property_key,
                            const char* controller,
                            base::Value* default_value) {
    DictionaryValue* property_dict;
    std::string key = base::StringPrintf("%s.%s",
                                         NetworkUIData::kKeyProperties,
                                         property_key);
    EXPECT_TRUE(dict->GetDictionary(key, &property_dict));
    ASSERT_TRUE(property_dict);
    std::string actual_controller;
    EXPECT_TRUE(property_dict->GetString(NetworkPropertyUIData::kKeyController,
                                         &actual_controller));
    EXPECT_EQ(controller, actual_controller);
    if (default_value) {
      base::Value* actual_value = NULL;
      EXPECT_TRUE(property_dict->Get(NetworkPropertyUIData::kKeyDefaultValue,
                                     &actual_value));
      EXPECT_TRUE(base::Value::Equals(default_value, actual_value));
    }
  }

  TestNetwork network_;

  DISALLOW_COPY_AND_ASSIGN(NetworkUIDataTest);
};

TEST_F(NetworkUIDataTest, ONCSource) {
  network_.ui_data()->SetString(NetworkUIData::kKeyONCSource, "user_import");
  EXPECT_EQ(NetworkUIData::ONC_SOURCE_USER_IMPORT,
            NetworkUIData::GetONCSource(&network_));
  EXPECT_FALSE(NetworkUIData::IsManaged(&network_));

  network_.ui_data()->SetString(NetworkUIData::kKeyONCSource, "device_policy");
  EXPECT_EQ(NetworkUIData::ONC_SOURCE_DEVICE_POLICY,
            NetworkUIData::GetONCSource(&network_));
  EXPECT_TRUE(NetworkUIData::IsManaged(&network_));

  network_.ui_data()->SetString(NetworkUIData::kKeyONCSource, "user_policy");
  EXPECT_EQ(NetworkUIData::ONC_SOURCE_USER_POLICY,
            NetworkUIData::GetONCSource(&network_));
  EXPECT_TRUE(NetworkUIData::IsManaged(&network_));
}

TEST_F(NetworkUIDataTest, ReadProperties) {
  SetProperty(network_.ui_data(), NetworkUIData::kPropertyAutoConnect,
              "policy", NULL);
  SetProperty(network_.ui_data(), NetworkUIData::kPropertyPreferred,
              "user", Value::CreateBooleanValue(true));
  SetProperty(network_.ui_data(), NetworkUIData::kPropertyPassphrase,
              NULL, Value::CreateIntegerValue(42));

  NetworkPropertyUIData property_ui_data(&network_,
                                         NetworkUIData::kPropertyAutoConnect);
  EXPECT_TRUE(property_ui_data.managed());
  EXPECT_FALSE(property_ui_data.recommended());
  EXPECT_FALSE(property_ui_data.editable());
  EXPECT_FALSE(property_ui_data.default_value());

  property_ui_data.UpdateFromNetwork(&network_,
                                     NetworkUIData::kPropertyPreferred);
  EXPECT_FALSE(property_ui_data.managed());
  EXPECT_TRUE(property_ui_data.recommended());
  EXPECT_TRUE(property_ui_data.editable());
  base::FundamentalValue expected_preferred(true);
  EXPECT_TRUE(base::Value::Equals(&expected_preferred,
                                  property_ui_data.default_value()));

  property_ui_data.UpdateFromNetwork(&network_,
                                     NetworkUIData::kPropertyPassphrase);
  EXPECT_FALSE(property_ui_data.managed());
  EXPECT_TRUE(property_ui_data.recommended());
  EXPECT_TRUE(property_ui_data.editable());
  base::FundamentalValue expected_passphrase(42);
  EXPECT_TRUE(base::Value::Equals(&expected_passphrase,
                                  property_ui_data.default_value()));

  property_ui_data.UpdateFromNetwork(&network_,
                                     NetworkUIData::kPropertySaveCredentials);
  EXPECT_FALSE(property_ui_data.managed());
  EXPECT_FALSE(property_ui_data.recommended());
  EXPECT_TRUE(property_ui_data.editable());
  EXPECT_FALSE(property_ui_data.default_value());
}

TEST_F(NetworkUIDataTest, WriteProperties) {
  NetworkUIData ui_data_builder;
  ui_data_builder.set_onc_source(NetworkUIData::ONC_SOURCE_USER_POLICY);
  NetworkPropertyUIData auto_connect_ui_data(
      NetworkPropertyUIData::CONTROLLER_USER,
      base::Value::CreateBooleanValue(true));
  ui_data_builder.SetProperty(NetworkUIData::kPropertyAutoConnect,
                              auto_connect_ui_data);
  NetworkPropertyUIData preferred_ui_data(
      NetworkPropertyUIData::CONTROLLER_POLICY,
      base::Value::CreateBooleanValue(42));
  ui_data_builder.SetProperty(NetworkUIData::kPropertyPreferred,
                              preferred_ui_data);
  NetworkPropertyUIData passphrase_ui_data(
      NetworkPropertyUIData::CONTROLLER_USER, NULL);
  ui_data_builder.SetProperty(NetworkUIData::kPropertyPassphrase,
                              passphrase_ui_data);

  DictionaryValue dict;
  ui_data_builder.FillDictionary(&dict);

  std::string onc_source;
  EXPECT_TRUE(dict.GetString(NetworkUIData::kKeyONCSource, &onc_source));
  EXPECT_EQ("user_policy", onc_source);
  CheckProperty(&dict, NetworkUIData::kPropertyAutoConnect,
                "user", base::Value::CreateBooleanValue(true));
  CheckProperty(&dict, NetworkUIData::kPropertyPreferred,
                "policy", base::Value::CreateBooleanValue(42));
  CheckProperty(&dict, NetworkUIData::kPropertyPassphrase,
                "user", NULL);
}

}  // namespace chromeos
