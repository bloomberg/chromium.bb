// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that a byte can be properly written and read. We only have this
// test for byte, as repeating this for other basic types is too redundant.
TEST(MessageTest, AppendAndPopByte) {
  dbus::Message message;
  message.reset_raw_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  dbus::MessageWriter writer(&message);
  writer.AppendByte(123);  // The input is 123.

  dbus::MessageReader reader(&message);
  ASSERT_TRUE(reader.HasMoreData());  // Should have data to read.
  ASSERT_EQ(dbus::Message::BYTE, reader.GetDataType());

  bool bool_value = false;
  // Should fail as the type is not bool here.
  ASSERT_FALSE(reader.PopBool(&bool_value));

  uint8 byte_value = 0;
  ASSERT_TRUE(reader.PopByte(&byte_value));
  EXPECT_EQ(123, byte_value);  // Should match with the input.
  ASSERT_FALSE(reader.HasMoreData());  // Should not have more data to read.

  // Try to get another byte. Should fail.
  ASSERT_FALSE(reader.PopByte(&byte_value));
}

// Check all basic types can be properly written and read.
TEST(MessageTest, AppendAndPopBasicDataTypes) {
  dbus::Message message;
  message.reset_raw_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  dbus::MessageWriter writer(&message);

  // Append 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path".
  writer.AppendByte(0);
  writer.AppendBool(true);
  writer.AppendInt16(2);
  writer.AppendUint16(3);
  writer.AppendInt32(4);
  writer.AppendUint32(5);
  writer.AppendInt64(6);
  writer.AppendUint64(7);
  writer.AppendDouble(8.0);
  writer.AppendString("string");
  writer.AppendObjectPath("/object/path");

  uint8 byte_value = 0;
  bool bool_value = false;
  int16 int16_value = 0;
  uint16 uint16_value = 0;
  int32 int32_value = 0;
  uint32 uint32_value = 0;
  int64 int64_value = 0;
  uint64 uint64_value = 0;
  double double_value = 0;
  std::string string_value;
  std::string object_path_value;

  dbus::MessageReader reader(&message);
  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_TRUE(reader.PopByte(&byte_value));
  ASSERT_TRUE(reader.PopBool(&bool_value));
  ASSERT_TRUE(reader.PopInt16(&int16_value));
  ASSERT_TRUE(reader.PopUint16(&uint16_value));
  ASSERT_TRUE(reader.PopInt32(&int32_value));
  ASSERT_TRUE(reader.PopUint32(&uint32_value));
  ASSERT_TRUE(reader.PopInt64(&int64_value));
  ASSERT_TRUE(reader.PopUint64(&uint64_value));
  ASSERT_TRUE(reader.PopDouble(&double_value));
  ASSERT_TRUE(reader.PopString(&string_value));
  ASSERT_TRUE(reader.PopObjectPath(&object_path_value));
  ASSERT_FALSE(reader.HasMoreData());

  // 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path" should be returned.
  EXPECT_EQ(0, byte_value);
  EXPECT_EQ(true, bool_value);
  EXPECT_EQ(2, int16_value);
  EXPECT_EQ(3U, uint16_value);
  EXPECT_EQ(4, int32_value);
  EXPECT_EQ(5U, uint32_value);
  EXPECT_EQ(6, int64_value);
  EXPECT_EQ(7U, uint64_value);
  EXPECT_DOUBLE_EQ(8.0, double_value);
  EXPECT_EQ("string", string_value);
  EXPECT_EQ("/object/path", object_path_value);
}

// Check all variant types can be properly written and read.
TEST(MessageTest, AppendAndPopVariantDataTypes) {
  dbus::Message message;
  message.reset_raw_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  dbus::MessageWriter writer(&message);

  // Append 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path".
  writer.AppendVariantOfByte(0);
  writer.AppendVariantOfBool(true);
  writer.AppendVariantOfInt16(2);
  writer.AppendVariantOfUint16(3);
  writer.AppendVariantOfInt32(4);
  writer.AppendVariantOfUint32(5);
  writer.AppendVariantOfInt64(6);
  writer.AppendVariantOfUint64(7);
  writer.AppendVariantOfDouble(8.0);
  writer.AppendVariantOfString("string");
  writer.AppendVariantOfObjectPath("/object/path");

  uint8 byte_value = 0;
  bool bool_value = false;
  int16 int16_value = 0;
  uint16 uint16_value = 0;
  int32 int32_value = 0;
  uint32 uint32_value = 0;
  int64 int64_value = 0;
  uint64 uint64_value = 0;
  double double_value = 0;
  std::string string_value;
  std::string object_path_value;

  dbus::MessageReader reader(&message);
  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_TRUE(reader.PopVariantOfByte(&byte_value));
  ASSERT_TRUE(reader.PopVariantOfBool(&bool_value));
  ASSERT_TRUE(reader.PopVariantOfInt16(&int16_value));
  ASSERT_TRUE(reader.PopVariantOfUint16(&uint16_value));
  ASSERT_TRUE(reader.PopVariantOfInt32(&int32_value));
  ASSERT_TRUE(reader.PopVariantOfUint32(&uint32_value));
  ASSERT_TRUE(reader.PopVariantOfInt64(&int64_value));
  ASSERT_TRUE(reader.PopVariantOfUint64(&uint64_value));
  ASSERT_TRUE(reader.PopVariantOfDouble(&double_value));
  ASSERT_TRUE(reader.PopVariantOfString(&string_value));
  ASSERT_TRUE(reader.PopVariantOfObjectPath(&object_path_value));
  ASSERT_FALSE(reader.HasMoreData());

  // 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path" should be returned.
  EXPECT_EQ(0, byte_value);
  EXPECT_EQ(true, bool_value);
  EXPECT_EQ(2, int16_value);
  EXPECT_EQ(3U, uint16_value);
  EXPECT_EQ(4, int32_value);
  EXPECT_EQ(5U, uint32_value);
  EXPECT_EQ(6, int64_value);
  EXPECT_EQ(7U, uint64_value);
  EXPECT_DOUBLE_EQ(8.0, double_value);
  EXPECT_EQ("string", string_value);
  EXPECT_EQ("/object/path", object_path_value);
}

TEST(MessageTest, ArrayOfBytes) {
  dbus::Message message;
  message.reset_raw_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  dbus::MessageWriter writer(&message);
  std::vector<uint8> bytes;
  bytes.push_back(1);
  bytes.push_back(2);
  bytes.push_back(3);
  writer.AppendArrayOfBytes(bytes.data(), bytes.size());

  dbus::MessageReader reader(&message);
  uint8* output_bytes = NULL;
  size_t length = 0;
  ASSERT_TRUE(reader.PopArrayOfBytes(&output_bytes, &length));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(3U, length);
  ASSERT_EQ(1, output_bytes[0]);
  ASSERT_EQ(2, output_bytes[1]);
  ASSERT_EQ(3, output_bytes[2]);
}

// Test that an array can be properly written and read. We only have this
// test for array, as repeating this for other container types is too
// redundant.
TEST(MessageTest, OpenArrayAndPopArray) {
  dbus::Message message;
  message.reset_raw_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  dbus::MessageWriter writer(&message);
  dbus::MessageWriter array_writer(&message);
  writer.OpenArray("s", &array_writer);  // Open an array of strings.
  array_writer.AppendString("foo");
  array_writer.AppendString("bar");
  array_writer.AppendString("baz");
  writer.CloseContainer(&array_writer);

  dbus::MessageReader reader(&message);
  ASSERT_EQ(dbus::Message::ARRAY, reader.GetDataType());
  dbus::MessageReader array_reader(&message);
  ASSERT_TRUE(reader.PopArray(&array_reader));
  ASSERT_FALSE(reader.HasMoreData());  // Should not have more data to read.

  std::string string_value;
  ASSERT_TRUE(array_reader.PopString(&string_value));
  EXPECT_EQ("foo", string_value);
  ASSERT_TRUE(array_reader.PopString(&string_value));
  EXPECT_EQ("bar", string_value);
  ASSERT_TRUE(array_reader.PopString(&string_value));
  EXPECT_EQ("baz", string_value);
  // Should not have more data to read.
  ASSERT_FALSE(array_reader.HasMoreData());
}

// Create a complex message using array, struct, variant, dict entry, and
// make sure it can be read properly.
TEST(MessageTest, CreateComplexMessageAndReadIt) {
  dbus::Message message;
  message.reset_raw_message(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));
  dbus::MessageWriter writer(&message);
  {
    dbus::MessageWriter array_writer(&message);
    // Open an array of variants.
    writer.OpenArray("v", &array_writer);
    {
      // The first value in the array.
      {
        dbus::MessageWriter variant_writer(&message);
        // Open a variant of a boolean.
        array_writer.OpenVariant("b", &variant_writer);
        variant_writer.AppendBool(true);
        array_writer.CloseContainer(&variant_writer);
      }

      // The second value in the array.
      {
        dbus::MessageWriter variant_writer(&message);
        // Open a variant of a struct that contains a string and an int32.
        array_writer.OpenVariant("(si)", &variant_writer);
        {
          dbus::MessageWriter struct_writer(&message);
          variant_writer.OpenStruct(&struct_writer);
          struct_writer.AppendString("string");
          struct_writer.AppendInt32(123);
          variant_writer.CloseContainer(&struct_writer);
        }
        array_writer.CloseContainer(&variant_writer);
      }

      // The third value in the array.
      {
        dbus::MessageWriter variant_writer(&message);
        // Open a variant of an array of string-to-int64 dict entries.
        array_writer.OpenVariant("a{sx}", &variant_writer);
        {
          // Opens an array of string-to-int64 dict entries.
          dbus::MessageWriter dict_array_writer(&message);
          variant_writer.OpenArray("{sx}", &dict_array_writer);
          {
            // Opens a string-to-int64 dict entries.
            dbus::MessageWriter dict_entry_writer(&message);
            dict_array_writer.OpenDictEntry(&dict_entry_writer);
            dict_entry_writer.AppendString("foo");
            dict_entry_writer.AppendInt64(1234567890123456789);
            dict_array_writer.CloseContainer(&dict_entry_writer);
          }
          variant_writer.CloseContainer(&dict_array_writer);
        }
        array_writer.CloseContainer(&variant_writer);
      }
    }
    writer.CloseContainer(&array_writer);
  }
  // What we have created looks like this:
  EXPECT_EQ("signature: av\n"
            "\n"
            "array [\n"
            "  variant     bool true\n"
            "  variant     struct {\n"
            "      string \"string\"\n"
            "      int32 123\n"
            "    }\n"
            "  variant     array [\n"
            "      dict entry {\n"
            "        string \"foo\"\n"
            "        int64 1234567890123456789\n"
            "      }\n"
            "    ]\n"
            "]\n",
            message.ToString());

  dbus::MessageReader reader(&message);
  dbus::MessageReader array_reader(&message);
  ASSERT_TRUE(reader.PopArray(&array_reader));

  // The first value in the array.
  bool bool_value = false;
  ASSERT_TRUE(array_reader.PopVariantOfBool(&bool_value));
  EXPECT_EQ(true, bool_value);

  // The second value in the array.
  {
    dbus::MessageReader variant_reader(&message);
    ASSERT_TRUE(array_reader.PopVariant(&variant_reader));
    {
      dbus::MessageReader struct_reader(&message);
      ASSERT_TRUE(variant_reader.PopStruct(&struct_reader));
      std::string string_value;
      ASSERT_TRUE(struct_reader.PopString(&string_value));
      EXPECT_EQ("string", string_value);
      int32 int32_value = 0;
      ASSERT_TRUE(struct_reader.PopInt32(&int32_value));
      EXPECT_EQ(123, int32_value);
      ASSERT_FALSE(struct_reader.HasMoreData());
    }
    ASSERT_FALSE(variant_reader.HasMoreData());
  }

  // The third value in the array.
  {
    dbus::MessageReader variant_reader(&message);
    ASSERT_TRUE(array_reader.PopVariant(&variant_reader));
    {
      dbus::MessageReader dict_array_reader(&message);
      ASSERT_TRUE(variant_reader.PopArray(&dict_array_reader));
      {
        dbus::MessageReader dict_entry_reader(&message);
        ASSERT_TRUE(dict_array_reader.PopDictEntry(&dict_entry_reader));
        std::string string_value;
        ASSERT_TRUE(dict_entry_reader.PopString(&string_value));
        EXPECT_EQ("foo", string_value);
        int64 int64_value = 0;
        ASSERT_TRUE(dict_entry_reader.PopInt64(&int64_value));
        EXPECT_EQ(1234567890123456789, int64_value);
      }
      ASSERT_FALSE(dict_array_reader.HasMoreData());
    }
    ASSERT_FALSE(variant_reader.HasMoreData());
  }
  ASSERT_FALSE(array_reader.HasMoreData());
  ASSERT_FALSE(reader.HasMoreData());
}

TEST(MessageTest, Message) {
  dbus::Message message;
  EXPECT_TRUE(message.raw_message() == NULL);
  EXPECT_EQ(dbus::Message::MESSAGE_INVALID, message.GetMessageType());
}

TEST(MessageTest, MethodCall) {
  dbus::MethodCall method_call("com.example.Interface", "SomeMethod");
  EXPECT_TRUE(method_call.raw_message() != NULL);
  EXPECT_EQ(dbus::Message::MESSAGE_METHOD_CALL, method_call.GetMessageType());
  method_call.SetServiceName("com.example.Service");
  method_call.SetObjectPath("/com/example/Object");

  dbus::MessageWriter writer(&method_call);
  writer.AppendString("payload");

  EXPECT_EQ("destination: com.example.Service\n"
            "path: /com/example/Object\n"
            "interface: com.example.Interface\n"
            "member: SomeMethod\n"
            "signature: s\n"
            "\n"
            "string \"payload\"\n",
            method_call.ToString());
}

TEST(MessageTest, Response) {
  dbus::Response response;
  EXPECT_TRUE(response.raw_message() == NULL);
  response.reset_raw_message(
      dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN));
  EXPECT_EQ(dbus::Message::MESSAGE_METHOD_RETURN, response.GetMessageType());
}

TEST(MessageTest, ToString_EmptyMessage) {
  dbus::Message message;
  EXPECT_EQ("", message.ToString());
}
