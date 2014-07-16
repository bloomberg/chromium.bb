// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data.h"

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(FormDataTest, SerializeAndDeserialize) {
  FormData data;
  data.name = base::ASCIIToUTF16("name");
  data.origin = GURL("origin");
  data.action = GURL("action");
  data.user_submitted = true;

  FormFieldData field_data;
  field_data.label = base::ASCIIToUTF16("label");
  field_data.name = base::ASCIIToUTF16("name");
  field_data.value = base::ASCIIToUTF16("value");
  field_data.form_control_type = "password";
  field_data.autocomplete_attribute = "off";
  field_data.max_length = 200;
  field_data.is_autofilled = true;
  field_data.is_checked = true;
  field_data.is_checkable = true;
  field_data.is_focusable = true;
  field_data.should_autocomplete = false;
  field_data.text_direction = base::i18n::RIGHT_TO_LEFT;
  field_data.option_values.push_back(base::ASCIIToUTF16("First"));
  field_data.option_values.push_back(base::ASCIIToUTF16("Second"));
  field_data.option_contents.push_back(base::ASCIIToUTF16("First"));
  field_data.option_contents.push_back(base::ASCIIToUTF16("Second"));

  data.fields.push_back(field_data);

  // Change a few fields.
  field_data.max_length = 150;
  field_data.option_values.push_back(base::ASCIIToUTF16("Third"));
  field_data.option_contents.push_back(base::ASCIIToUTF16("Third"));
  data.fields.push_back(field_data);

  Pickle pickle;
  SerializeFormData(data, &pickle);

  PickleIterator iter(pickle);
  FormData actual;
  EXPECT_TRUE(DeserializeFormData(&iter, &actual));

  EXPECT_EQ(actual, data);
}

}  // namespace autofill
