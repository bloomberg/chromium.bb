// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <utility>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/common_param_traits.h"
#include "chrome/common/geoposition.h"
#include "gfx/rect.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "printing/backend/print_backend.h"
#include "printing/native_metafile.h"
#include "printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Tests that serialize/deserialize correctly understand each other
TEST(IPCMessageTest, Serialize) {
  const char* serialize_cases[] = {
    "http://www.google.com/",
    "http://user:pass@host.com:888/foo;bar?baz#nop",
    "#inva://idurl/",
  };

  for (size_t i = 0; i < arraysize(serialize_cases); i++) {
    GURL input(serialize_cases[i]);
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<GURL>::Write(&msg, input);

    GURL output;
    void* iter = NULL;
    EXPECT_TRUE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));

    // We want to test each component individually to make sure its range was
    // correctly serialized and deserialized, not just the spec.
    EXPECT_EQ(input.possibly_invalid_spec(), output.possibly_invalid_spec());
    EXPECT_EQ(input.is_valid(), output.is_valid());
    EXPECT_EQ(input.scheme(), output.scheme());
    EXPECT_EQ(input.username(), output.username());
    EXPECT_EQ(input.password(), output.password());
    EXPECT_EQ(input.host(), output.host());
    EXPECT_EQ(input.port(), output.port());
    EXPECT_EQ(input.path(), output.path());
    EXPECT_EQ(input.query(), output.query());
    EXPECT_EQ(input.ref(), output.ref());
  }

  // Also test the corrupt case.
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  msg.WriteInt(99);
  GURL output;
  void* iter = NULL;
  EXPECT_FALSE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));
}

// Tests std::pair serialization
TEST(IPCMessageTest, Pair) {
  typedef std::pair<std::string, std::string> TestPair;

  TestPair input("foo", "bar");
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<TestPair>::Write(&msg, input);

  TestPair output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ParamTraits<TestPair>::Read(&msg, &iter, &output));
  EXPECT_EQ(output.first, "foo");
  EXPECT_EQ(output.second, "bar");

}

// Tests bitmap serialization.
TEST(IPCMessageTest, Bitmap) {
  SkBitmap bitmap;

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 5);
  bitmap.allocPixels();
  memset(bitmap.getPixels(), 'A', bitmap.getSize());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<SkBitmap>::Write(&msg, bitmap);

  SkBitmap output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ParamTraits<SkBitmap>::Read(&msg, &iter, &output));

  EXPECT_EQ(bitmap.config(), output.config());
  EXPECT_EQ(bitmap.width(), output.width());
  EXPECT_EQ(bitmap.height(), output.height());
  EXPECT_EQ(bitmap.rowBytes(), output.rowBytes());
  EXPECT_EQ(bitmap.getSize(), output.getSize());
  EXPECT_EQ(memcmp(bitmap.getPixels(), output.getPixels(), bitmap.getSize()),
            0);

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  // Copy the first message block over to |bad_msg|.
  const char* fixed_data;
  int fixed_data_size;
  iter = NULL;
  msg.ReadData(&iter, &fixed_data, &fixed_data_size);
  bad_msg.WriteData(fixed_data, fixed_data_size);
  // Add some bogus pixel data.
  const size_t bogus_pixels_size = bitmap.getSize() * 2;
  scoped_array<char> bogus_pixels(new char[bogus_pixels_size]);
  memset(bogus_pixels.get(), 'B', bogus_pixels_size);
  bad_msg.WriteData(bogus_pixels.get(), bogus_pixels_size);
  // Make sure we don't read out the bitmap!
  SkBitmap bad_output;
  iter = NULL;
  EXPECT_FALSE(IPC::ParamTraits<SkBitmap>::Read(&bad_msg, &iter, &bad_output));
}

TEST(IPCMessageTest, ListValue) {
  ListValue input;
  input.Set(0, Value::CreateDoubleValue(42.42));
  input.Set(1, Value::CreateStringValue("forty"));
  input.Set(2, Value::CreateNullValue());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  ListValue output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = NULL;
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

TEST(IPCMessageTest, DictionaryValue) {
  DictionaryValue input;
  input.Set("null", Value::CreateNullValue());
  input.Set("bool", Value::CreateBooleanValue(true));
  input.Set("int", Value::CreateIntegerValue(42));

  scoped_ptr<DictionaryValue> subdict(new DictionaryValue());
  subdict->Set("str", Value::CreateStringValue("forty two"));
  subdict->Set("bool", Value::CreateBooleanValue(false));

  scoped_ptr<ListValue> sublist(new ListValue());
  sublist->Set(0, Value::CreateDoubleValue(42.42));
  sublist->Set(1, Value::CreateStringValue("forty"));
  sublist->Set(2, Value::CreateStringValue("two"));
  subdict->Set("list", sublist.release());

  input.Set("dict", subdict.release());

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  DictionaryValue output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = NULL;
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

TEST(IPCMessageTest, Geoposition) {
  Geoposition input;
  input.latitude = 0.1;
  input.longitude = 51.3;
  input.accuracy = 13.7;
  input.altitude = 42.24;
  input.altitude_accuracy = 9.3;
  input.speed = 55;
  input.heading = 120;
  input.timestamp = base::Time::FromInternalValue(1977);
  input.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  input.error_message = "unittest error message for geoposition";

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  Geoposition output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(input.altitude, output.altitude);
  EXPECT_EQ(input.altitude_accuracy, output.altitude_accuracy);
  EXPECT_EQ(input.latitude, output.latitude);
  EXPECT_EQ(input.longitude, output.longitude);
  EXPECT_EQ(input.accuracy, output.accuracy);
  EXPECT_EQ(input.heading, output.heading);
  EXPECT_EQ(input.speed, output.speed);
  EXPECT_EQ(input.error_code, output.error_code);
  EXPECT_EQ(input.error_message, output.error_message);

  std::string log_message;
  IPC::LogParam(output, &log_message);
  EXPECT_STREQ("<Geoposition>"
               "0.100000 51.300000 13.700000 42.240000 "
               "9.300000 55.000000 120.000000 "
               "1977 unittest error message for geoposition"
               "<Geoposition::ErrorCode>2",
               log_message.c_str());
}

// Tests printing::PageRange serialization
TEST(IPCMessageTest, PageRange) {
  printing::PageRange input;
  input.from = 2;
  input.to = 45;
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<printing::PageRange>::Write(&msg, input);

  printing::PageRange output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ParamTraits<printing::PageRange>::Read(
      &msg, &iter, &output));
  EXPECT_TRUE(input == output);
}

// Tests printing::NativeMetafile serialization.
// TODO(sanjeevr): Make this test meaningful for non-Windows platforms. We
// need to initialize the metafile using alternate means on the other OSes.
#if defined(OS_WIN)
TEST(IPCMessageTest, Metafile) {
  printing::NativeMetafile metafile;
  RECT test_rect = {0, 0, 100, 100};
  // Create a metsfile using the screen DC as a reference.
  metafile.CreateDc(NULL, NULL);
  metafile.CloseDc();

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<printing::NativeMetafile>::Write(&msg, metafile);

  printing::NativeMetafile output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ParamTraits<printing::NativeMetafile>::Read(
      &msg, &iter, &output));

  EXPECT_EQ(metafile.GetDataSize(), output.GetDataSize());
  EXPECT_EQ(metafile.GetBounds(), output.GetBounds());
  EXPECT_EQ(::GetDeviceCaps(metafile.hdc(), LOGPIXELSX),
      ::GetDeviceCaps(output.hdc(), LOGPIXELSX));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  // Write some bogus metafile data.
  const size_t bogus_data_size = metafile.GetDataSize() * 2;
  scoped_array<char> bogus_data(new char[bogus_data_size]);
  memset(bogus_data.get(), 'B', bogus_data_size);
  bad_msg.WriteData(bogus_data.get(), bogus_data_size);
  // Make sure we don't read out the metafile!
  printing::NativeMetafile bad_output;
  iter = NULL;
  EXPECT_FALSE(IPC::ParamTraits<printing::NativeMetafile>::Read(
      &bad_msg, &iter, &bad_output));
}
#endif  // defined(OS_WIN)

// Tests printing::PrinterCapsAndDefaults serialization
TEST(IPCMessageTest, PrinterCapsAndDefaults) {
  printing::PrinterCapsAndDefaults input;
  input.printer_capabilities = "Test Capabilities";
  input.caps_mime_type = "text/plain";
  input.printer_defaults = "Test Defaults";
  input.defaults_mime_type = "text/plain";

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<printing::PrinterCapsAndDefaults>::Write(&msg, input);

  printing::PrinterCapsAndDefaults output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ParamTraits<printing::PrinterCapsAndDefaults>::Read(
      &msg, &iter, &output));
  EXPECT_TRUE(input.printer_capabilities == output.printer_capabilities);
  EXPECT_TRUE(input.caps_mime_type == output.caps_mime_type);
  EXPECT_TRUE(input.printer_defaults == output.printer_defaults);
  EXPECT_TRUE(input.defaults_mime_type == output.defaults_mime_type);
}

