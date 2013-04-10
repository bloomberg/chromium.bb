// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_property_ui_data.h"

#include "base/values.h"
#include "chromeos/network/network_ui_data.h"
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

TEST_F(NetworkUIDataTest, PropertyInit) {
  NetworkPropertyUIData empty_prop;
  CheckProperty(empty_prop, NULL, false, false, true);

  NetworkUIData empty_data;
  NetworkPropertyUIData null_prop(empty_data);
  CheckProperty(null_prop, NULL, false, false, true);

  base::DictionaryValue empty_dict;
  NetworkUIData empty_data_2(empty_dict);
  NetworkPropertyUIData empty_dict_prop(empty_data_2);
  CheckProperty(empty_dict_prop, NULL, false, false, true);

}

TEST_F(NetworkUIDataTest, ParseOncProperty) {
  base::DictionaryValue ui_data_dict;
  NetworkUIData ui_data;

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
  recommended.Append(new base::StringValue("b"));
  recommended.Append(new base::StringValue("c"));
  recommended.Append(new base::StringValue("a.a"));
  onc.Set("Recommended", recommended.DeepCopy());
  onc.Set("a.Recommended", recommended.DeepCopy());

  NetworkPropertyUIData prop;

  ui_data.set_onc_source(onc::ONC_SOURCE_USER_IMPORT);
  ui_data.FillDictionary(&ui_data_dict);

  NetworkUIData empty_data;
  prop.ParseOncProperty(empty_data, &onc, "a");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(ui_data, &onc, "a");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(ui_data, &onc, "a.b");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(ui_data, &onc, "c");
  CheckProperty(prop, NULL, false, false, true);

  ui_data.set_onc_source(onc::ONC_SOURCE_USER_POLICY);
  ui_data.FillDictionary(&ui_data_dict);

  prop.ParseOncProperty(ui_data, &onc, "a");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(ui_data, &onc, "b");
  CheckProperty(prop, &val_b, false, true, true);

  prop.ParseOncProperty(ui_data, &onc, "c");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(ui_data, &onc, "d");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(ui_data, &onc, "a.a");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(ui_data, &onc, "a.b");
  CheckProperty(prop, &val_a_b, false, true, true);

  prop.ParseOncProperty(ui_data, &onc, "a.c");
  CheckProperty(prop, NULL, false, false, true);

  prop.ParseOncProperty(ui_data, &onc, "a.d");
  CheckProperty(prop, NULL, true, false, false);

  prop.ParseOncProperty(ui_data, NULL, "a.e");
  CheckProperty(prop, NULL, true, false, false);
}

}  // namespace chromeos
