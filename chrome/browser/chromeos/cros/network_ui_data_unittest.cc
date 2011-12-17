// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class NetworkUIDataTest : public testing::Test {
 protected:
  NetworkUIDataTest() {}
  virtual ~NetworkUIDataTest() {}

  void CheckProperty(const NetworkPropertyUIData& property,
                     const base::Value* expected_default_value,
                     bool expected_managed,
                     bool expected_recommended,
                     bool expected_editable) {
    if (expected_default_value) {
      EXPECT_TRUE(base::Value::Equals(expected_default_value,
                                      property.default_value()));
    } else {
      EXPECT_FALSE(property.default_value());
    }
    EXPECT_EQ(expected_managed, property.managed());
    EXPECT_EQ(expected_recommended, property.recommended());
    EXPECT_EQ(expected_editable, property.editable());
  }
};

TEST_F(NetworkUIDataTest, ONCSource) {
  base::DictionaryValue ui_data;

  ui_data.SetString(NetworkUIData::kKeyONCSource, "user_import");
  EXPECT_EQ(NetworkUIData::ONC_SOURCE_USER_IMPORT,
            NetworkUIData::GetONCSource(&ui_data));
  EXPECT_FALSE(NetworkUIData::IsManaged(&ui_data));

  ui_data.SetString(NetworkUIData::kKeyONCSource, "device_policy");
  EXPECT_EQ(NetworkUIData::ONC_SOURCE_DEVICE_POLICY,
            NetworkUIData::GetONCSource(&ui_data));
  EXPECT_TRUE(NetworkUIData::IsManaged(&ui_data));

  ui_data.SetString(NetworkUIData::kKeyONCSource, "user_policy");
  EXPECT_EQ(NetworkUIData::ONC_SOURCE_USER_POLICY,
            NetworkUIData::GetONCSource(&ui_data));
  EXPECT_TRUE(NetworkUIData::IsManaged(&ui_data));
}

TEST_F(NetworkUIDataTest, PropertyInit) {
  NetworkPropertyUIData empty_prop;
  CheckProperty(empty_prop, NULL, false, false, true);

  NetworkPropertyUIData null_prop(NULL);
  CheckProperty(null_prop, NULL, false, false, true);

  base::DictionaryValue empty_dict;
  NetworkPropertyUIData empty_dict_prop(&empty_dict);
  CheckProperty(empty_dict_prop, NULL, false, false, true);

}

TEST_F(NetworkUIDataTest, ParseOncProperty) {
  base::DictionaryValue ui_data;
  NetworkUIData ui_data_builder;

  base::DictionaryValue onc;

  base::StringValue val_a("a");
  base::StringValue val_b("b");
  base::StringValue val_a_a("a_a");
  base::StringValue val_a_b("a_b");

  onc.Set("a", val_a.DeepCopy());
  onc.Set("b", val_b.DeepCopy());
  onc.Set("a.a", val_a_a.DeepCopy());
  onc.Set("a.b", val_a_b.DeepCopy());
  base::ListValue recommended;
  recommended.Append(base::Value::CreateStringValue("b"));
  recommended.Append(base::Value::CreateStringValue("c"));
  recommended.Append(base::Value::CreateStringValue("a.a"));
  onc.Set("Recommended", recommended.DeepCopy());
  onc.Set("a.Recommended", recommended.DeepCopy());

  NetworkPropertyUIData prop;

  ui_data_builder.set_onc_source(NetworkUIData::ONC_SOURCE_USER_IMPORT);
  ui_data_builder.FillDictionary(&ui_data);

  prop.ParseOncProperty(NULL, &onc, "a");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(&ui_data, &onc, "a");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(&ui_data, &onc, "a.b");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(&ui_data, &onc, "c");
  CheckProperty(prop, NULL, false, false, true);

  ui_data_builder.set_onc_source(NetworkUIData::ONC_SOURCE_USER_POLICY);
  ui_data_builder.FillDictionary(&ui_data);

  prop.ParseOncProperty(&ui_data, &onc, "a");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(&ui_data, &onc, "b");
  CheckProperty(prop, &val_b, false, true, true);

  prop.ParseOncProperty(&ui_data, &onc, "c");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(&ui_data, &onc, "d");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(&ui_data, &onc, "a.a");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(&ui_data, &onc, "a.b");
  CheckProperty(prop, &val_a_b, false, true, true);

  prop.ParseOncProperty(&ui_data, &onc, "a.c");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(&ui_data, &onc, "a.d");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(&ui_data, NULL, "a.e");
  CheckProperty(prop, NULL, true, false, false);
}

}  // namespace chromeos
