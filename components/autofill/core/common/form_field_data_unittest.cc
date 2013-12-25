// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include "base/i18n/rtl.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(FormFieldDataTest, SerializeAndDeserialize) {
  FormFieldData data;
  data.label = base::ASCIIToUTF16("label");
  data.name = base::ASCIIToUTF16("name");
  data.value = base::ASCIIToUTF16("value");
  data.form_control_type = "password";
  data.autocomplete_attribute = "off";
  data.max_length = 200;
  data.is_autofilled = true;
  data.is_checked = true;
  data.is_checkable = true;
  data.is_focusable = true;
  data.should_autocomplete = false;
  data.text_direction = base::i18n::RIGHT_TO_LEFT;
  data.option_values.push_back(base::ASCIIToUTF16("First"));
  data.option_values.push_back(base::ASCIIToUTF16("Second"));
  data.option_contents.push_back(base::ASCIIToUTF16("First"));
  data.option_contents.push_back(base::ASCIIToUTF16("Second"));

  Pickle pickle;
  SerializeFormFieldData(data, &pickle);

  PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_EQ(actual, data);
}

}  // namespace autofill
