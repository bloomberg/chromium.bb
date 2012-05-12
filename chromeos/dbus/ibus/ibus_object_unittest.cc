// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Add more test case, especially fail case.

#include "chromeos/dbus/ibus/ibus_object.h"

#include <string>
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
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
  const uint32 kSampleArrayOfUint32Count = 10UL;

  // Create ibus object.
  IBusObjectWriter ibus_object_writer(kSampleTypeName1, "suauv", &writer);
  ibus_object_writer.AppendString(kSampleText1);
  ibus_object_writer.AppendUint32(kSampleUint32);
  dbus::MessageWriter array_writer(NULL);
  ibus_object_writer.OpenArray("u", &array_writer);
  for (uint32 i = 0; i < kSampleArrayOfUint32Count; ++i)
    array_writer.AppendUint32(i);
  ibus_object_writer.CloseContainer(&array_writer);
  IBusObjectWriter ibus_nested_object_writer(kSampleTypeName2, "s", NULL);
  ibus_object_writer.AppendIBusObject(&ibus_nested_object_writer);
  ibus_nested_object_writer.AppendString(kSampleText2);
  ibus_object_writer.CloseAll();

  // Read ibus_object.
  dbus::MessageReader reader(message.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName1, &reader);
  ASSERT_TRUE(ibus_object_reader.Init());
  // Check the first string value.
  std::string expected_string;
  ASSERT_TRUE(ibus_object_reader.PopString(&expected_string));
  EXPECT_EQ(kSampleText1, expected_string);
  // Check the second uint32 value.
  uint32 expected_uint32;
  ASSERT_TRUE(ibus_object_reader.PopUint32(&expected_uint32));
  EXPECT_EQ(kSampleUint32, expected_uint32);
  // Check the third value which is array of uint32.
  dbus::MessageReader array_reader(NULL);
  ASSERT_TRUE(ibus_object_reader.PopArray(&array_reader));
  for (uint32 i = 0; i < kSampleArrayOfUint32Count; ++i) {
    uint32 expected_uint32;
    ASSERT_TRUE(array_reader.PopUint32(&expected_uint32));
    EXPECT_EQ(i, expected_uint32);
  }
  // Check the fourth value which is IBusObject.
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
  ibus_object_writer.CloseAll();

  // Read empty IBusObject.
  dbus::MessageReader reader(message.get());
  IBusObjectReader ibus_object_reader(kSampleTypeName, &reader);
  ASSERT_TRUE(ibus_object_reader.Init());
  EXPECT_FALSE(ibus_object_reader.HasMoreData());
}

}  // namespace chromeos
