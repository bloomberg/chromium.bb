// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_message_handler.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(WebUIMessageHandlerTest, ExtractIntegerValue) {
  ListValue list;
  int value, zero_value = 0, neg_value = -1234, pos_value = 1234;
  string16 zero_string(UTF8ToUTF16("0"));
  string16 neg_string(UTF8ToUTF16("-1234"));
  string16 pos_string(UTF8ToUTF16("1234"));

  list.Append(new base::FundamentalValue(zero_value));
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, zero_value);
  list.Clear();

  list.Append(new base::FundamentalValue(neg_value));
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, neg_value);
  list.Clear();

  list.Append(new base::FundamentalValue(pos_value));
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, pos_value);
  list.Clear();

  list.Append(new base::StringValue(zero_string));
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, zero_value);
  list.Clear();

  list.Append(new base::StringValue(neg_string));
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, neg_value);
  list.Clear();

  list.Append(new base::StringValue(pos_string));
  EXPECT_TRUE(WebUIMessageHandler::ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, pos_value);
}

TEST(WebUIMessageHandlerTest, ExtractDoubleValue) {
  base::ListValue list;
  double value, zero_value = 0.0, neg_value = -1234.5, pos_value = 1234.5;
  string16 zero_string(UTF8ToUTF16("0"));
  string16 neg_string(UTF8ToUTF16("-1234.5"));
  string16 pos_string(UTF8ToUTF16("1234.5"));

  list.Append(new base::FundamentalValue(zero_value));
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, zero_value);
  list.Clear();

  list.Append(new base::FundamentalValue(neg_value));
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, neg_value);
  list.Clear();

  list.Append(new base::FundamentalValue(pos_value));
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, pos_value);
  list.Clear();

  list.Append(new base::StringValue(zero_string));
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, zero_value);
  list.Clear();

  list.Append(new base::StringValue(neg_string));
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, neg_value);
  list.Clear();

  list.Append(new base::StringValue(pos_string));
  EXPECT_TRUE(WebUIMessageHandler::ExtractDoubleValue(&list, &value));
  EXPECT_DOUBLE_EQ(value, pos_value);
}

TEST(WebUIMessageHandlerTest, ExtractStringValue) {
  base::ListValue list;
  string16 in_string(UTF8ToUTF16(
      "The facts, though interesting, are irrelevant."));
  list.Append(new base::StringValue(in_string));
  string16 out_string = WebUIMessageHandler::ExtractStringValue(&list);
  EXPECT_EQ(in_string, out_string);
}

}  // namespace content
