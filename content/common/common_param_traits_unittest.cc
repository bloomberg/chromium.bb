// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/host_port_pair.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

// Tests that serialize/deserialize correctly understand each other
TEST(IPCMessageTest, Serialize) {
  const char* serialize_cases[] = {
    "http://www.google.com/",
    "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (size_t i = 0; i < arraysize(serialize_cases); i++) {
    GURL input(serialize_cases[i]);
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<GURL>::Write(&msg, input);

    GURL output;
    PickleIterator iter(msg);
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

  // Test an invalid GURL.
  {
    IPC::Message msg;
    msg.WriteString("#inva://idurl/");
    GURL output;
    PickleIterator iter(msg);
    EXPECT_FALSE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));
  }

  // Also test the corrupt case.
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  msg.WriteInt(99);
  GURL output;
  PickleIterator iter(msg);
  EXPECT_FALSE(IPC::ParamTraits<GURL>::Read(&msg, &iter, &output));
}

// Tests std::pair serialization
TEST(IPCMessageTest, Pair) {
  typedef std::pair<std::string, std::string> TestPair;

  TestPair input("foo", "bar");
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<TestPair>::Write(&msg, input);

  TestPair output;
  PickleIterator iter(msg);
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
  PickleIterator iter(msg);
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
  iter = PickleIterator(msg);
  msg.ReadData(&iter, &fixed_data, &fixed_data_size);
  bad_msg.WriteData(fixed_data, fixed_data_size);
  // Add some bogus pixel data.
  const size_t bogus_pixels_size = bitmap.getSize() * 2;
  scoped_array<char> bogus_pixels(new char[bogus_pixels_size]);
  memset(bogus_pixels.get(), 'B', bogus_pixels_size);
  bad_msg.WriteData(bogus_pixels.get(), bogus_pixels_size);
  // Make sure we don't read out the bitmap!
  SkBitmap bad_output;
  iter = PickleIterator(bad_msg);
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
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = PickleIterator(bad_msg);
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
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_TRUE(input.Equals(&output));

  // Also test the corrupt case.
  IPC::Message bad_msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  bad_msg.WriteInt(99);
  iter = PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

// Tests net::HostPortPair serialization
TEST(IPCMessageTest, HostPortPair) {
  net::HostPortPair input("host.com", 12345);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::ParamTraits<net::HostPortPair>::Write(&msg, input);

  net::HostPortPair output;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<net::HostPortPair>::Read(&msg, &iter, &output));
  EXPECT_EQ(input.host(), output.host());
  EXPECT_EQ(input.port(), output.port());
}
