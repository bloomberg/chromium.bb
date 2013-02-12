// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/values_util.h"

#include <vector>

#include "base/float_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ValuesUtilTest, PopBasicTypes) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append basic type values.
  dbus::MessageWriter writer(response.get());
  const uint8 kByteValue = 42;
  writer.AppendByte(kByteValue);
  const bool kBoolValue = true;
  writer.AppendBool(kBoolValue);
  const int16 kInt16Value = -43;
  writer.AppendInt16(kInt16Value);
  const uint16 kUint16Value = 44;
  writer.AppendUint16(kUint16Value);
  const int32 kInt32Value = -45;
  writer.AppendInt32(kInt32Value);
  const uint32 kUint32Value = 46;
  writer.AppendUint32(kUint32Value);
  const int64 kInt64Value = -47;
  writer.AppendInt64(kInt64Value);
  const uint64 kUint64Value = 48;
  writer.AppendUint64(kUint64Value);
  const double kDoubleValue = 4.9;
  writer.AppendDouble(kDoubleValue);
  const std::string kStringValue = "fifty";
  writer.AppendString(kStringValue);
  const std::string kEmptyStringValue;
  writer.AppendString(kEmptyStringValue);
  const dbus::ObjectPath kObjectPathValue("/ObjectPath");
  writer.AppendObjectPath(kObjectPathValue);

  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value;
  scoped_ptr<base::Value> expected_value;
  // Pop a byte.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kByteValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a bool.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kBoolValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop an int16.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kInt16Value));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a uint16.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kUint16Value));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop an int32.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kInt32Value));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a uint32.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(
      new base::FundamentalValue(static_cast<double>(kUint32Value)));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop an int64.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(
      new base::FundamentalValue(static_cast<double>(kInt64Value)));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a uint64.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(
      new base::FundamentalValue(static_cast<double>(kUint64Value)));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a double.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kDoubleValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a string.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::StringValue(kStringValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop an empty string.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::StringValue(kEmptyStringValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop an object path.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::StringValue(kObjectPathValue.value()));
  EXPECT_TRUE(value->Equals(expected_value.get()));
}

TEST(ValuesUtilTest, PopVariant) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append variant values.
  dbus::MessageWriter writer(response.get());
  const bool kBoolValue = true;
  writer.AppendVariantOfBool(kBoolValue);
  const int32 kInt32Value = -45;
  writer.AppendVariantOfInt32(kInt32Value);
  const double kDoubleValue = 4.9;
  writer.AppendVariantOfDouble(kDoubleValue);
  const std::string kStringValue = "fifty";
  writer.AppendVariantOfString(kStringValue);

  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value;
  scoped_ptr<base::Value> expected_value;
  // Pop a bool.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kBoolValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop an int32.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kInt32Value));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a double.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::FundamentalValue(kDoubleValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  // Pop a string.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(new base::StringValue(kStringValue));
  EXPECT_TRUE(value->Equals(expected_value.get()));
}

// Pop extremely large integers which cannot be precisely represented in
// double.
TEST(ValuesUtilTest, PopExtremelyLargeIntegers) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append large integers.
  dbus::MessageWriter writer(response.get());
  const int64 kInt64Value = -123456789012345689LL;
  writer.AppendInt64(kInt64Value);
  const uint64 kUint64Value = 9876543210987654321ULL;
  writer.AppendUint64(kUint64Value);

  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value;
  scoped_ptr<base::Value> expected_value;
  double double_value = 0;
  // Pop an int64.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(
      new base::FundamentalValue(static_cast<double>(kInt64Value)));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  ASSERT_TRUE(value->GetAsDouble(&double_value));
  EXPECT_NE(kInt64Value, static_cast<int64>(double_value));
  // Pop a uint64.
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  expected_value.reset(
      new base::FundamentalValue(static_cast<double>(kUint64Value)));
  EXPECT_TRUE(value->Equals(expected_value.get()));
  ASSERT_TRUE(value->GetAsDouble(&double_value));
  EXPECT_NE(kUint64Value, static_cast<uint64>(double_value));
}

TEST(ValuesUtilTest, PopIntArray) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append an int32 array.
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter sub_writer(NULL);
  std::vector<int32> data;
  data.push_back(0);
  data.push_back(1);
  data.push_back(2);
  writer.OpenArray("i", &sub_writer);
  for (size_t i = 0; i != data.size(); ++i)
    sub_writer.AppendInt32(data[i]);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  scoped_ptr<base::ListValue> list_value(new base::ListValue);
  for (size_t i = 0; i != data.size(); ++i)
    list_value->Append(new base::FundamentalValue(data[i]));

  // Pop an int32 array.
  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(list_value.get()));
}

TEST(ValuesUtilTest, PopStringArray) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append a string array.
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter sub_writer(NULL);
  std::vector<std::string> data;
  data.push_back("Dreamlifter");
  data.push_back("Beluga");
  data.push_back("Mriya");
  writer.AppendArrayOfStrings(data);

  // Create the expected value.
  scoped_ptr<base::ListValue> list_value(new base::ListValue);
  for (size_t i = 0; i != data.size(); ++i)
    list_value->Append(new base::StringValue(data[i]));

  // Pop a string array.
  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(list_value.get()));
}

TEST(ValuesUtilTest, PopStruct) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append a struct.
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter sub_writer(NULL);
  writer.OpenStruct(&sub_writer);
  const bool kBoolValue = true;
  sub_writer.AppendBool(kBoolValue);
  const int32 kInt32Value = -123;
  sub_writer.AppendInt32(kInt32Value);
  const double kDoubleValue = 1.23;
  sub_writer.AppendDouble(kDoubleValue);
  const std::string kStringValue = "one two three";
  sub_writer.AppendString(kStringValue);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::ListValue list_value;
  list_value.Append(new base::FundamentalValue(kBoolValue));
  list_value.Append(new base::FundamentalValue(kInt32Value));
  list_value.Append(new base::FundamentalValue(kDoubleValue));
  list_value.Append(new base::StringValue(kStringValue));

  // Pop a struct.
  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&list_value));
}

TEST(ValuesUtilTest, PopStringToVariantDictionary) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append a dictionary.
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter sub_writer(NULL);
  dbus::MessageWriter entry_writer(NULL);
  writer.OpenArray("{sv}", &sub_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey1 = "one";
  entry_writer.AppendString(kKey1);
  const bool kBoolValue = true;
  entry_writer.AppendVariantOfBool(kBoolValue);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey2 = "two";
  entry_writer.AppendString(kKey2);
  const int32 kInt32Value = -45;
  entry_writer.AppendVariantOfInt32(kInt32Value);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey3 = "three";
  entry_writer.AppendString(kKey3);
  const double kDoubleValue = 4.9;
  entry_writer.AppendVariantOfDouble(kDoubleValue);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey4 = "four";
  entry_writer.AppendString(kKey4);
  const std::string kStringValue = "fifty";
  entry_writer.AppendVariantOfString(kStringValue);
  sub_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::DictionaryValue dictionary_value;
  dictionary_value.SetBoolean(kKey1, kBoolValue);
  dictionary_value.SetInteger(kKey2, kInt32Value);
  dictionary_value.SetDouble(kKey3, kDoubleValue);
  dictionary_value.SetString(kKey4, kStringValue);

  // Pop a dictinoary.
  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&dictionary_value));
}

TEST(ValuesUtilTest, PopDictionaryWithDottedStringKey) {
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Append a dictionary.
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter sub_writer(NULL);
  dbus::MessageWriter entry_writer(NULL);
  writer.OpenArray("{sv}", &sub_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey1 = "www.example.com";  // String including dots.
  entry_writer.AppendString(kKey1);
  const bool kBoolValue = true;
  entry_writer.AppendVariantOfBool(kBoolValue);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey2 = ".example";  // String starting with a dot.
  entry_writer.AppendString(kKey2);
  const int32 kInt32Value = -45;
  entry_writer.AppendVariantOfInt32(kInt32Value);
  sub_writer.CloseContainer(&entry_writer);
  sub_writer.OpenDictEntry(&entry_writer);
  const std::string kKey3 = "example.";  // String ending with a dot.
  entry_writer.AppendString(kKey3);
  const double kDoubleValue = 4.9;
  entry_writer.AppendVariantOfDouble(kDoubleValue);
  sub_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::DictionaryValue dictionary_value;
  dictionary_value.SetWithoutPathExpansion(
      kKey1, new base::FundamentalValue(kBoolValue));
  dictionary_value.SetWithoutPathExpansion(
      kKey2, new base::FundamentalValue(kInt32Value));
  dictionary_value.SetWithoutPathExpansion(
      kKey3, new base::FundamentalValue(kDoubleValue));

  // Pop a dictinoary.
  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&dictionary_value));
}

TEST(ValuesUtilTest, PopDoubleToIntDictionary) {
  // Create test data.
  const int32 kValues[] = {0, 1, 1, 2, 3, 5, 8, 13, 21};
  const std::vector<int32> values(kValues, kValues + arraysize(kValues));
  std::vector<double> keys(values.size());
  for (size_t i = 0; i != values.size(); ++i)
    keys[i] = sqrt(values[i]);

  // Append a dictionary.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter sub_writer(NULL);
  writer.OpenArray("{di}", &sub_writer);
  for (size_t i = 0; i != values.size(); ++i) {
    dbus::MessageWriter entry_writer(NULL);
    sub_writer.OpenDictEntry(&entry_writer);
    entry_writer.AppendDouble(keys[i]);
    entry_writer.AppendInt32(values[i]);
    sub_writer.CloseContainer(&entry_writer);
  }
  writer.CloseContainer(&sub_writer);

  // Create the expected value.
  base::DictionaryValue dictionary_value;
  for (size_t i = 0; i != values.size(); ++i) {
    scoped_ptr<base::Value> key_value(new base::FundamentalValue(keys[i]));
    std::string key_string;
    base::JSONWriter::Write(key_value.get(), &key_string);
    dictionary_value.SetWithoutPathExpansion(
        key_string, new base::FundamentalValue(values[i]));
  }

  // Pop a dictionary.
  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&dictionary_value));
}

TEST(ValuesUtilTest, AppendBasicTypes) {
  const base::FundamentalValue kBoolValue(false);
  const base::FundamentalValue kIntegerValue(42);
  const base::FundamentalValue kDoubleValue(4.2);
  const base::StringValue kStringValue("string");

  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendBasicTypeValueData(&writer, kBoolValue);
  AppendBasicTypeValueData(&writer, kIntegerValue);
  AppendBasicTypeValueData(&writer, kDoubleValue);
  AppendBasicTypeValueData(&writer, kStringValue);

  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value;
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kBoolValue));
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kIntegerValue));
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kDoubleValue));
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kStringValue));
}

TEST(ValuesUtilTest, AppendBasicTypesAsVariant) {
  const base::FundamentalValue kBoolValue(false);
  const base::FundamentalValue kIntegerValue(42);
  const base::FundamentalValue kDoubleValue(4.2);
  const base::StringValue kStringValue("string");

  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendBasicTypeValueDataAsVariant(&writer, kBoolValue);
  AppendBasicTypeValueDataAsVariant(&writer, kIntegerValue);
  AppendBasicTypeValueDataAsVariant(&writer, kDoubleValue);
  AppendBasicTypeValueDataAsVariant(&writer, kStringValue);

  dbus::MessageReader reader(response.get());
  scoped_ptr<base::Value> value;
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kBoolValue));
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kIntegerValue));
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kDoubleValue));
  value.reset(dbus::PopDataAsValue(&reader));
  ASSERT_TRUE(value.get() != NULL);
  EXPECT_TRUE(value->Equals(&kStringValue));
}
