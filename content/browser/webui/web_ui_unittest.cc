// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui.h"

#include "base/string16.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  TestWebUIMessageHandler() {}
  virtual ~TestWebUIMessageHandler() {}

 protected:
  virtual void RegisterMessages() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIMessageHandler);
};

TEST(WebUIMessageHandlerTest, ExtractIntegerValue) {
  TestWebUIMessageHandler handler;

  ListValue list;
  int value, zero_value = 0, neg_value = -1234, pos_value = 1234;

  list.Append(Value::CreateIntegerValue(zero_value));
  EXPECT_TRUE(handler.ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, zero_value);
  list.Clear();

  list.Append(Value::CreateIntegerValue(neg_value));
  EXPECT_TRUE(handler.ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, neg_value);
  list.Clear();

  list.Append(Value::CreateIntegerValue(pos_value));
  EXPECT_TRUE(handler.ExtractIntegerValue(&list, &value));
  EXPECT_EQ(value, pos_value);
}

TEST(WebUIMessageHandlerTest, ExtractDoubleValue) {
  TestWebUIMessageHandler handler;

  ListValue list;
  double value, zero_value = 0.0, neg_value = -1234.5, pos_value = 1234.5;

  list.Append(Value::CreateDoubleValue(zero_value));
  EXPECT_TRUE(handler.ExtractDoubleValue(&list, &value));
  EXPECT_EQ(value, zero_value);
  list.Clear();

  list.Append(Value::CreateDoubleValue(neg_value));
  EXPECT_TRUE(handler.ExtractDoubleValue(&list, &value));
  EXPECT_EQ(value, neg_value);
  list.Clear();

  list.Append(Value::CreateDoubleValue(pos_value));
  EXPECT_TRUE(handler.ExtractDoubleValue(&list, &value));
  EXPECT_EQ(value, pos_value);
}

TEST(WebUIMessageHandlerTest, ExtractStringValue) {
  TestWebUIMessageHandler handler;

  ListValue list;
  string16 in_string = "The facts, though interesting, are irrelevant."
  list.Append(Value::CreateStringValue(string));
  string16 out_string = handler.ExtractStringValue(&list);
  EXPECT_STREQ(in_string.c_str(), out_string.c_str());
}

