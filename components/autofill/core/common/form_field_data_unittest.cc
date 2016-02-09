// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include "base/i18n/rtl.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void FillCommonFields(FormFieldData* data) {
  data->label = base::ASCIIToUTF16("label");
  data->name = base::ASCIIToUTF16("name");
  data->value = base::ASCIIToUTF16("value");
  data->form_control_type = "password";
  data->autocomplete_attribute = "off";
  data->max_length = 200;
  data->is_autofilled = true;
  data->is_checked = true;
  data->is_checkable = true;
  data->is_focusable = true;
  data->should_autocomplete = false;
  data->text_direction = base::i18n::RIGHT_TO_LEFT;
  data->option_values.push_back(base::ASCIIToUTF16("First"));
  data->option_values.push_back(base::ASCIIToUTF16("Second"));
  data->option_contents.push_back(base::ASCIIToUTF16("First"));
  data->option_contents.push_back(base::ASCIIToUTF16("Second"));
}

void WriteCommonSection1(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteString16(data.label);
  pickle->WriteString16(data.name);
  pickle->WriteString16(data.value);
  pickle->WriteString(data.form_control_type);
  pickle->WriteString(data.autocomplete_attribute);
  pickle->WriteUInt32(data.max_length);
  pickle->WriteBool(data.is_autofilled);
  pickle->WriteBool(data.is_checked);
  pickle->WriteBool(data.is_checkable);
  pickle->WriteBool(data.is_focusable);
  pickle->WriteBool(data.should_autocomplete);
}

void WriteCommonSection2(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteInt(data.text_direction);
  pickle->WriteInt(static_cast<int>(data.option_values.size()));
  for (auto s : data.option_values)
    pickle->WriteString16(s);
  pickle->WriteInt(static_cast<int>(data.option_contents.size()));
  for (auto s : data.option_contents)
    pickle->WriteString16(s);
}

}  // namespace

TEST(FormFieldDataTest, SerializeAndDeserialize) {
  FormFieldData data;
  FillCommonFields(&data);
  data.role = FormFieldData::ROLE_ATTRIBUTE_PRESENTATION;

  base::Pickle pickle;
  SerializeFormFieldData(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion1) {
  FormFieldData data;
  FillCommonFields(&data);

  base::Pickle pickle;
  pickle.WriteInt(1);
  WriteCommonSection1(data, &pickle);
  WriteCommonSection2(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

// Verify that if the data isn't valid, the FormFieldData isn't populated
// during deserialization.
TEST(FormFieldDataTest, DeserializeBadData) {
  base::Pickle pickle;
  pickle.WriteInt(255);
  pickle.WriteString16(base::ASCIIToUTF16("random"));
  pickle.WriteString16(base::ASCIIToUTF16("data"));

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_FALSE(DeserializeFormFieldData(&iter, &actual));

  FormFieldData empty;
  EXPECT_TRUE(actual.SameFieldAs(empty));
}

}  // namespace autofill
