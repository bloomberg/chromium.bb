// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data.h"

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This function serializes the form data into the pickle in version one format.
// It should always be possible to deserialize it using DeserializeFormData(),
// even when version changes. See kPickleVersion in form_data.cc.
void SerializeInVersion1Format(const autofill::FormData& form_data,
                               Pickle* pickle) {
  pickle->WriteInt(1);
  pickle->WriteString16(form_data.name);
  base::string16 method(base::ASCIIToUTF16("POST"));
  pickle->WriteString16(method);
  pickle->WriteString(form_data.origin.spec());
  pickle->WriteString(form_data.action.spec());
  pickle->WriteBool(form_data.user_submitted);
  pickle->WriteInt(static_cast<int>(form_data.fields.size()));
  for (size_t i = 0; i < form_data.fields.size(); ++i) {
    SerializeFormFieldData(form_data.fields[i], pickle);
  }
}

// This function serializes the form data into the pickle in incorrect format
// (no version number).
void SerializeIncorrectFormat(const autofill::FormData& form_data,
                               Pickle* pickle) {
  pickle->WriteString16(form_data.name);
  pickle->WriteString(form_data.origin.spec());
  pickle->WriteString(form_data.action.spec());
  pickle->WriteBool(form_data.user_submitted);
  pickle->WriteInt(static_cast<int>(form_data.fields.size()));
  for (size_t i = 0; i < form_data.fields.size(); ++i) {
    SerializeFormFieldData(form_data.fields[i], pickle);
  }
}

}

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

TEST(FormDataTest, Serialize_v1_Deserialize_vCurrent) {
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
  SerializeInVersion1Format(data, &pickle);

  PickleIterator iter(pickle);
  FormData actual;
  EXPECT_TRUE(DeserializeFormData(&iter, &actual));

  EXPECT_EQ(actual, data);
}

TEST(FormDataTest, SerializeIncorrectFormatAndDeserialize) {
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
  SerializeIncorrectFormat(data, &pickle);

  PickleIterator iter(pickle);
  FormData actual;
  EXPECT_FALSE(DeserializeFormData(&iter, &actual));
}

}  // namespace autofill
