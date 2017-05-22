// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_utils.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/unguessable_token.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {
namespace {

// Tests nesting of messages as parameters to other messages.
TEST(IPCMessageUtilsTest, NestedMessages) {
  int32_t nested_routing = 12;
  uint32_t nested_type = 78;
  int nested_content = 456789;
  Message::PriorityValue nested_priority = Message::PRIORITY_HIGH;
  Message nested_msg(nested_routing, nested_type, nested_priority);
  nested_msg.set_sync();
  ParamTraits<int>::Write(&nested_msg, nested_content);

  // Outer message contains the nested one as its parameter.
  int32_t outer_routing = 91;
  uint32_t outer_type = 88;
  Message::PriorityValue outer_priority = Message::PRIORITY_NORMAL;
  Message outer_msg(outer_routing, outer_type, outer_priority);
  ParamTraits<Message>::Write(&outer_msg, nested_msg);

  // Read back the nested message.
  base::PickleIterator iter(outer_msg);
  IPC::Message result_msg;
  ASSERT_TRUE(ParamTraits<Message>::Read(&outer_msg, &iter, &result_msg));

  // Verify nested message headers.
  EXPECT_EQ(nested_msg.routing_id(), result_msg.routing_id());
  EXPECT_EQ(nested_msg.type(), result_msg.type());
  EXPECT_EQ(nested_msg.priority(), result_msg.priority());
  EXPECT_EQ(nested_msg.flags(), result_msg.flags());

  // Verify nested message content
  base::PickleIterator nested_iter(nested_msg);
  int result_content = 0;
  ASSERT_TRUE(ParamTraits<int>::Read(&nested_msg, &nested_iter,
                                     &result_content));
  EXPECT_EQ(nested_content, result_content);

  // Try reading past the ends for both messages and make sure it fails.
  IPC::Message dummy;
  ASSERT_FALSE(ParamTraits<Message>::Read(&outer_msg, &iter, &dummy));
  ASSERT_FALSE(ParamTraits<int>::Read(&nested_msg, &nested_iter,
                                      &result_content));
}

// Tests that detection of various bad parameters is working correctly.
TEST(IPCMessageUtilsTest, ParameterValidation) {
  base::FilePath::StringType ok_string(FILE_PATH_LITERAL("hello"), 5);
  base::FilePath::StringType bad_string(FILE_PATH_LITERAL("hel\0o"), 5);

  // Change this if ParamTraits<FilePath>::Write() changes.
  IPC::Message message;
  ParamTraits<base::FilePath::StringType>::Write(&message, ok_string);
  ParamTraits<base::FilePath::StringType>::Write(&message, bad_string);

  base::PickleIterator iter(message);
  base::FilePath ok_path;
  base::FilePath bad_path;
  ASSERT_TRUE(ParamTraits<base::FilePath>::Read(&message, &iter, &ok_path));
  ASSERT_FALSE(ParamTraits<base::FilePath>::Read(&message, &iter, &bad_path));
}


TEST(IPCMessageUtilsTest, StackVector) {
  static const size_t stack_capacity = 5;
  base::StackVector<double, stack_capacity> stack_vector;
  for (size_t i = 0; i < 2 * stack_capacity; i++)
    stack_vector->push_back(i * 2.0);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, stack_vector);

  base::StackVector<double, stack_capacity> output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  for (size_t i = 0; i < 2 * stack_capacity; i++)
    EXPECT_EQ(stack_vector[i], output[i]);
}

// Tests that PickleSizer and Pickle agree on the size of a complex base::Value.
TEST(IPCMessageUtilsTest, ValueSize) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetIntegerWithoutPathExpansion("foo", 42);
  value->SetDoubleWithoutPathExpansion("bar", 3.14);
  value->SetStringWithoutPathExpansion("baz", "hello");
  value->SetWithoutPathExpansion("qux", base::MakeUnique<base::Value>());

  std::unique_ptr<base::DictionaryValue> nested_dict(new base::DictionaryValue);
  nested_dict->SetIntegerWithoutPathExpansion("foobar", 5);
  value->SetWithoutPathExpansion("nested", std::move(nested_dict));

  std::unique_ptr<base::ListValue> list_value(new base::ListValue);
  list_value->AppendString("im a string");
  list_value->AppendString("im another string");
  value->SetWithoutPathExpansion("awesome-list", std::move(list_value));

  base::Pickle pickle;
  IPC::WriteParam(&pickle, *value);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, *value);

  EXPECT_EQ(sizer.payload_size(), pickle.payload_size());
}

TEST(IPCMessageUtilsTest, JsonValueSize) {
  const char kJson[] = "[ { \"foo\": \"bar\", \"baz\": 1234.0 } ]";
  std::unique_ptr<base::Value> json_value = base::JSONReader::Read(kJson);
  EXPECT_NE(nullptr, json_value);
  base::ListValue value;
  value.Append(std::move(json_value));

  base::Pickle pickle;
  IPC::WriteParam(&pickle, value);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, value);

  EXPECT_EQ(sizer.payload_size(), pickle.payload_size());
}

TEST(IPCMessageUtilsTest, MojoChannelHandle) {
  mojo::MessagePipe message_pipe;
  IPC::ChannelHandle channel_handle(message_pipe.handle0.release());

  IPC::Message message;
  IPC::WriteParam(&message, channel_handle);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, channel_handle);
  EXPECT_EQ(sizer.payload_size(), message.payload_size());

  base::PickleIterator iter(message);
  IPC::ChannelHandle result_handle;
  EXPECT_TRUE(IPC::ReadParam(&message, &iter, &result_handle));
  EXPECT_EQ(channel_handle.mojo_handle, result_handle.mojo_handle);
}

TEST(IPCMessageUtilsTest, OptionalUnset) {
  base::Optional<int> opt;
  base::Pickle pickle;
  IPC::WriteParam(&pickle, opt);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, opt);

  EXPECT_EQ(sizer.payload_size(), pickle.payload_size());

  std::string log;
  IPC::LogParam(opt, &log);
  EXPECT_EQ("(unset)", log);

  base::Optional<int> unserialized_opt;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &unserialized_opt));
  EXPECT_FALSE(unserialized_opt);
}

TEST(IPCMessageUtilsTest, OptionalSet) {
  base::Optional<int> opt(10);
  base::Pickle pickle;
  IPC::WriteParam(&pickle, opt);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, opt);

  EXPECT_EQ(sizer.payload_size(), pickle.payload_size());

  std::string log;
  IPC::LogParam(opt, &log);
  EXPECT_EQ("10", log);

  base::Optional<int> unserialized_opt;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &unserialized_opt));
  EXPECT_TRUE(unserialized_opt);
  EXPECT_EQ(opt.value(), unserialized_opt.value());
}

TEST(IPCMessageUtilsTest, SharedMemoryHandle) {
  base::SharedMemoryCreateOptions options;
  options.size = 1004;
  base::SharedMemory shmem;
  ASSERT_TRUE(shmem.Create(options));

  base::SharedMemoryHandle pre_pickle = shmem.handle().Duplicate();
  ASSERT_TRUE(pre_pickle.IsValid());

  IPC::Message message;
  IPC::WriteParam(&message, pre_pickle);

  base::SharedMemoryHandle post_pickle;
  base::PickleIterator iter(message);
  EXPECT_TRUE(IPC::ReadParam(&message, &iter, &post_pickle));
  EXPECT_EQ(pre_pickle.GetGUID(), post_pickle.GetGUID());
  EXPECT_EQ(pre_pickle.GetSize(), post_pickle.GetSize());
}

TEST(IPCMessageUtilsTest, UnguessableTokenTest) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  base::Pickle pickle;
  IPC::WriteParam(&pickle, token);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, token);

  EXPECT_EQ(sizer.payload_size(), pickle.payload_size());

  std::string log;
  IPC::LogParam(token, &log);
  EXPECT_EQ(token.ToString(), log);

  base::UnguessableToken deserialized_token;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &deserialized_token));
  EXPECT_EQ(token, deserialized_token);
}

TEST(IPCMessageUtilsTest, FlatMap) {
  base::flat_map<std::string, int> input;
  input["foo"] = 42;
  input["bar"] = 96;

  base::Pickle pickle;
  IPC::WriteParam(&pickle, input);

  base::PickleSizer sizer;
  IPC::GetParamSize(&sizer, input);

  EXPECT_EQ(sizer.payload_size(), pickle.payload_size());

  base::PickleIterator iter(pickle);
  base::flat_map<std::string, int> output;
  EXPECT_TRUE(IPC::ReadParam(&pickle, &iter, &output));

  EXPECT_EQ(input, output);
}

}  // namespace
}  // namespace IPC
