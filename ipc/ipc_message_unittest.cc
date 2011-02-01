// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message.h"

#include <string.h>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
