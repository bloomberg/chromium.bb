// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more test case, especially fail case.

#include "chromeos/dbus/ibus/ibus_object.h"

#include <string>
#include <vector>
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(IBusObjectTest, WriteReadTest) {
  scoped_ptr<dbus::Response> message(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(message.get());

  const char kSampleTypeName1[] = "Sample Type Name 1";
  const char kSampleTypeName2[] = "Sample Type Name 2";
  const char kSampleText1[] = "Sample Text 1";
  const char kSampleText2[] = "Sample Text 2";
  const uint32 kSampleUint32 = 12345UL;
  const int32 kSampleInt32 = 54321;
  const bool kSampleBool = false;
  const uint32 kSampleArrayOfUint32Count = 10UL;
  const char kAttachmentKey[] = "key";
  const char kStringAttachment[] = "Attachment String Value";

  // Create ibus object.
  IBusObjectWriter ibus_object_writer(kSampleTypeName1, "suibauv", &writer);

  // Write attachment field.
  scoped_ptr<base::Value> write_value(
      base::Value::CreateStringValue(kStringAttachment));
  ibus_object_writer.AddAttachment(kAttachmentKey, *write_value.get());
  ibus_object_writer.CloseHeader();

  // Write contents values.
  ibus_object_writer.AppendString(kSampleText1);
  ibus_object_writer.AppendUint32(kSampleUint32);
  ibus_object_writer.AppendInt32(kSampleInt32);
  ibus_object_writer.AppendBool(kSampleBool);
  dbus::MessageWriter array_writer(NULL);
  ibus_object_writer.OpenArray("u", &array_writer);
  for (uint32 i = 0; i < kSampleArrayOfUint32Count; ++i)
    array_writer.AppendUint32(i);
  ibus_object_writer.CloseContainer(&array_writer);
  IBusObjectWriter ibus_nested_object_writer(kSampleTypeName2, "s", NULL);
  ibus_object_writer.AppendIBusObject(&ibus_nested_object_writer);
  ibus_nested_object_writer.CloseHeader();
  ibus_nested_object_writer.AppendString(kSampleText2);
  ibus_object_writer.CloseAll();

  // Read ibus_object.
  dbus::MessageReader reader(message.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName1, &reader);
  ASSERT_TRUE(ibus_object_reader.Init());
  // Check the attachment value;
  const base::Value* read_value =
      ibus_object_reader.GetAttachment(kAttachmentKey);
  ASSERT_TRUE(read_value);
  std::string attachment_value;
  ASSERT_TRUE(read_value->GetAsString(&attachment_value));
  EXPECT_EQ(kStringAttachment, attachment_value);
  // Check the first string value.
  std::string expected_string;
  ASSERT_TRUE(ibus_object_reader.PopString(&expected_string));
  EXPECT_EQ(kSampleText1, expected_string);
  // Check the second uint32 value.
  uint32 expected_uint32 = 0UL;
  ASSERT_TRUE(ibus_object_reader.PopUint32(&expected_uint32));
  EXPECT_EQ(kSampleUint32, expected_uint32);
  // Check the third int value.
  int32 expected_int32 = 0;
  ASSERT_TRUE(ibus_object_reader.PopInt32(&expected_int32));
  EXPECT_EQ(kSampleInt32, expected_int32);
  // Check the fourth boolean value.
  bool expected_bool = true;
  ASSERT_TRUE(ibus_object_reader.PopBool(&expected_bool));
  EXPECT_TRUE(kSampleBool == expected_bool);
  // Check the fifth value which is array of uint32.
  dbus::MessageReader array_reader(NULL);
  ASSERT_TRUE(ibus_object_reader.PopArray(&array_reader));
  for (uint32 i = 0; i < kSampleArrayOfUint32Count; ++i) {
    uint32 expected_uint32 = 0;
    ASSERT_TRUE(array_reader.PopUint32(&expected_uint32));
    EXPECT_EQ(i, expected_uint32);
  }
  // Check the sixth value which is IBusObject.
  IBusObjectReader ibus_nested_object_reader(kSampleTypeName2, NULL);
  ibus_object_reader.PopIBusObject(&ibus_nested_object_reader);
  std::string expected_text2;
  ASSERT_TRUE(ibus_nested_object_reader.PopString(&expected_text2));
  EXPECT_EQ(kSampleText2, expected_text2);

  EXPECT_FALSE(ibus_nested_object_reader.HasMoreData());
  EXPECT_FALSE(ibus_object_reader.HasMoreData());
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(IBusObjectTest, EmptyEntryTest) {
  const char kSampleTypeName[] = "Empty IBusObject Name";
  scoped_ptr<dbus::Response> message(dbus::Response::CreateEmpty());

  // Write empty IBusObject.
  dbus::MessageWriter writer(message.get());
  IBusObjectWriter ibus_object_writer(kSampleTypeName, "", &writer);
  ibus_object_writer.CloseHeader();
  ibus_object_writer.CloseAll();

  // Read empty IBusObject.
  dbus::MessageReader reader(message.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName, &reader);
  ASSERT_TRUE(ibus_object_reader.Init());
  EXPECT_FALSE(ibus_object_reader.HasMoreData());
}

TEST(IBusObjectTest, PopAppendIBusTextTest) {
  const char kSampleTypeName[] = "Empty IBusObject Name";
  const char kSampleString[] = "Sapmle String";
  IBusText::SelectionAttribute selection_attribute;
  selection_attribute.start_index = 0UL;
  selection_attribute.end_index = 10UL;
  scoped_ptr<dbus::Response> message(dbus::Response::CreateEmpty());

  // Write IBusText.
  dbus::MessageWriter writer(message.get());
  IBusObjectWriter ibus_object_writer(kSampleTypeName, "v", &writer);
  ibus_object_writer.CloseHeader();
  IBusText ibus_text;
  ibus_text.mutable_selection_attributes()->push_back(selection_attribute);
  ibus_text.set_text(kSampleString);
  ibus_object_writer.AppendIBusText(ibus_text);
  ibus_object_writer.CloseAll();

  // Read IBusText;
  dbus::MessageReader reader(message.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName, &reader);
  IBusText result_text;
  ASSERT_TRUE(ibus_object_reader.Init());
  ASSERT_TRUE(ibus_object_reader.PopIBusText(&result_text));
  EXPECT_FALSE(ibus_object_reader.HasMoreData());
  EXPECT_EQ(kSampleString, result_text.text());
  const std::vector<IBusText::SelectionAttribute>& selection_attributes =
      result_text.selection_attributes();
  ASSERT_EQ(1UL, selection_attributes.size());
  EXPECT_EQ(selection_attribute.start_index,
            selection_attributes[0].start_index);
  EXPECT_EQ(selection_attribute.end_index,
            selection_attributes[0].end_index);
}

TEST(IBusObjectTest, PopAppendStringAsIBusText) {
  const char kSampleTypeName[] = "Empty IBusObject Name";
  const char kSampleString[] = "Sapmle String";
  scoped_ptr<dbus::Response> message(dbus::Response::CreateEmpty());

  // Write string as IBusText.
  dbus::MessageWriter writer(message.get());
  IBusObjectWriter ibus_object_writer(kSampleTypeName, "v", &writer);
  ibus_object_writer.CloseHeader();
  ibus_object_writer.AppendStringAsIBusText(kSampleString);
  ibus_object_writer.CloseAll();

  // Read string from IBusText.
  dbus::MessageReader reader(message.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName, &reader);
  std::string result_str;
  ASSERT_TRUE(ibus_object_reader.Init());
  ASSERT_TRUE(ibus_object_reader.PopStringFromIBusText(&result_str));
  EXPECT_FALSE(ibus_object_reader.HasMoreData());
  EXPECT_EQ(kSampleString, result_str);
}

TEST(IBusObjectTest, PopAppendIBusPropertyTest) {
  const char kSampleTypeName[] = "Empty IBusObject Name";
  const char kSampleKey[] = "Key";
  const IBusProperty::IBusPropertyType kSampleType =
      IBusProperty::IBUS_PROPERTY_TYPE_MENU;
  const char kSampleLabel[] = "Label";
  const char kSampleTooltip[] = "Tooltip";
  const bool kSampleVisible = true;
  const bool kSampleChecked = false;
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Create IBusProperty.
  IBusProperty property;
  property.set_key(kSampleKey);
  property.set_type(kSampleType);
  property.set_label(kSampleLabel);
  property.set_tooltip(kSampleTooltip);
  property.set_visible(kSampleVisible);
  property.set_checked(kSampleChecked);

  // Write a IBusProperty.
  dbus::MessageWriter writer(response.get());
  IBusObjectWriter ibus_object_writer(kSampleTypeName, "v", &writer);
  ibus_object_writer.CloseHeader();
  ibus_object_writer.AppendIBusProperty(property);
  ibus_object_writer.CloseAll();

  // Read string from IBusText.
  dbus::MessageReader reader(response.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName, &reader);
  IBusProperty result_property;
  ASSERT_TRUE(ibus_object_reader.Init());
  ASSERT_TRUE(ibus_object_reader.PopIBusProperty(&result_property));
  EXPECT_EQ(kSampleKey, result_property.key());
  EXPECT_EQ(kSampleType, result_property.type());
  EXPECT_EQ(kSampleLabel, result_property.label());
  EXPECT_EQ(kSampleTooltip, result_property.tooltip());
  EXPECT_EQ(kSampleVisible, result_property.visible());
  EXPECT_TRUE(kSampleChecked == result_property.checked());
}

}  // namespace chromeos
