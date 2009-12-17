// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the command parser.

#include "gpu/command_buffer/service/precompile.h"

#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::_;

// Test fixture for CommandParser test - Creates a mock AsyncAPIInterface, and
// a fixed size memory buffer. Also provides a simple API to create a
// CommandParser.
class CommandParserTest : public testing::Test {
 protected:
  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    buffer_entry_count_ = 20;
    buffer_.reset(new CommandBufferEntry[buffer_entry_count_]);
  }
  virtual void TearDown() {}

  // Adds a DoCommand expectation in the mock.
  void AddDoCommandExpect(parse_error::ParseError _return,
                          unsigned int command,
                          unsigned int arg_count,
                          CommandBufferEntry *args) {
    EXPECT_CALL(*api_mock(), DoCommand(command, arg_count,
        Truly(AsyncAPIMock::IsArgs(arg_count, args))))
        .InSequence(sequence_)
        .WillOnce(Return(_return));
  }

  // Creates a parser, with a buffer of the specified size (in entries).
  CommandParser *MakeParser(unsigned int entry_count) {
    size_t shm_size = buffer_entry_count_ *
                      sizeof(CommandBufferEntry);  // NOLINT
    size_t command_buffer_size = entry_count *
                                 sizeof(CommandBufferEntry);  // NOLINT
    DCHECK_LE(command_buffer_size, shm_size);
    return new CommandParser(buffer(),
                             shm_size,
                             0,
                             command_buffer_size,
                             0,
                             api_mock());
  }

  unsigned int buffer_entry_count() { return 20; }
  AsyncAPIMock *api_mock() { return api_mock_.get(); }
  CommandBufferEntry *buffer() { return buffer_.get(); }
 private:
  unsigned int buffer_entry_count_;
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_array<CommandBufferEntry> buffer_;
  Sequence sequence_;
};

// Tests initialization conditions.
TEST_F(CommandParserTest, TestInit) {
  scoped_ptr<CommandParser> parser(MakeParser(10));
  EXPECT_EQ(0u, parser->get());
  EXPECT_EQ(0u, parser->put());
  EXPECT_TRUE(parser->IsEmpty());
}

// Tests simple commands.
TEST_F(CommandParserTest, TestSimple) {
  scoped_ptr<CommandParser> parser(MakeParser(10));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // add a single command, no args
  header.size = 1;
  header.command = 123;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  AddDoCommandExpect(parse_error::kParseNoError, 123, 0, NULL);
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessCommand());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add a single command, 2 args
  header.size = 3;
  header.command = 456;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 2134;
  buffer()[put++].value_float = 1.f;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  CommandBufferEntry param_array[2];
  param_array[0].value_int32 = 2134;
  param_array[1].value_float = 1.f;
  AddDoCommandExpect(parse_error::kParseNoError, 456, 2, param_array);
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessCommand());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests having multiple commands in the buffer.
TEST_F(CommandParserTest, TestMultipleCommands) {
  scoped_ptr<CommandParser> parser(MakeParser(10));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // add 2 commands, test with single ProcessCommand()
  header.size = 2;
  header.command = 789;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5151;

  CommandBufferOffset put_cmd2 = put;
  header.size = 2;
  header.command = 876;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 3434;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  CommandBufferEntry param_array[2];
  param_array[0].value_int32 = 5151;
  AddDoCommandExpect(parse_error::kParseNoError, 789, 1, param_array);
  param_array[1].value_int32 = 3434;
  AddDoCommandExpect(parse_error::kParseNoError, 876, 1,
                     param_array+1);

  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessCommand());
  EXPECT_EQ(put_cmd2, parser->get());
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessCommand());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 2 commands again, test with ProcessAllCommands()
  header.size = 2;
  header.command = 123;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5656;

  header.size = 2;
  header.command = 321;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 7878;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  param_array[0].value_int32 = 5656;
  AddDoCommandExpect(parse_error::kParseNoError, 123, 1, param_array);
  param_array[1].value_int32 = 7878;
  AddDoCommandExpect(parse_error::kParseNoError, 321, 1,
                     param_array+1);

  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests that the parser will wrap correctly at the end of the buffer.
TEST_F(CommandParserTest, TestWrap) {
  scoped_ptr<CommandParser> parser(MakeParser(5));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // add 3 commands with no args (1 word each)
  for (unsigned int i = 0; i < 3; ++i) {
    header.size = 1;
    header.command = i;
    buffer()[put++].value_header = header;
    AddDoCommandExpect(parse_error::kParseNoError, i, 0, NULL);
  }
  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 1 command with 1 arg (2 words). That should put us at the end of the
  // buffer.
  header.size = 2;
  header.command = 3;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5;
  CommandBufferEntry param;
  param.value_int32 = 5;
  AddDoCommandExpect(parse_error::kParseNoError, 3, 1, &param);

  DCHECK_EQ(5u, put);
  put = 0;
  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 1 command with 1 arg (2 words).
  header.size = 2;
  header.command = 4;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 6;
  param.value_int32 = 6;
  AddDoCommandExpect(parse_error::kParseNoError, 4, 1, &param);
  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests error conditions.
TEST_F(CommandParserTest, TestError) {
  scoped_ptr<CommandParser> parser(MakeParser(5));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // Generate a command with size 0.
  header.size = 0;
  header.command = 3;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(parse_error::kParseInvalidSize,
            parser->ProcessAllCommands());
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());

  parser.reset(MakeParser(5));
  put = parser->put();

  // Generate a command with size 6, extends beyond the end of the buffer.
  header.size = 6;
  header.command = 3;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(parse_error::kParseOutOfBounds,
            parser->ProcessAllCommands());
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());

  parser.reset(MakeParser(5));
  put = parser->put();

  // Generates 2 commands.
  header.size = 1;
  header.command = 3;
  buffer()[put++].value_header = header;
  CommandBufferOffset put_post_fail = put;
  header.size = 1;
  header.command = 4;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  // have the first command fail to parse.
  AddDoCommandExpect(parse_error::kParseUnknownCommand, 3, 0, NULL);
  EXPECT_EQ(parse_error::kParseUnknownCommand,
            parser->ProcessAllCommands());
  // check that only one command was executed, and that get reflects that
  // correctly.
  EXPECT_EQ(put_post_fail, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
  // make the second one succeed, and check that the parser recovered fine.
  AddDoCommandExpect(parse_error::kParseNoError, 4, 0, NULL);
  EXPECT_EQ(parse_error::kParseNoError, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

}  // namespace gpu
