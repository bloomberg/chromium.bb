/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains definitions for mock objects, used for testing.

// TODO: This file "manually" defines some mock objects. Using gMock
// would be definitely preferable, unfortunately it doesn't work on Windows
// yet.

#ifndef GPU_COMMAND_BUFFER_SERVICE_CROSS_MOCKS_H_
#define GPU_COMMAND_BUFFER_SERVICE_CROSS_MOCKS_H_

#include <vector>
#include "testing/gmock/include/gmock/gmock.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"

namespace command_buffer {

// Mocks an AsyncAPIInterface, using GMock.
class AsyncAPIMock : public AsyncAPIInterface {
 public:
  AsyncAPIMock() {
    testing::DefaultValue<parse_error::ParseError>::Set(
        parse_error::kParseNoError);
  }

  // Predicate that matches args passed to DoCommand, by looking at the values.
  class IsArgs {
   public:
    IsArgs(unsigned int arg_count, const void* args)
        : arg_count_(arg_count),
          args_(static_cast<CommandBufferEntry*>(const_cast<void*>(args))) {
    }

    bool operator() (const void* _args) const {
      const CommandBufferEntry* args =
          static_cast<const CommandBufferEntry*>(_args) + 1;
      for (unsigned int i = 0; i < arg_count_; ++i) {
        if (args[i].value_uint32 != args_[i].value_uint32) return false;
      }
      return true;
    }

   private:
    unsigned int arg_count_;
    CommandBufferEntry *args_;
  };

  MOCK_METHOD3(DoCommand, parse_error::ParseError(
      unsigned int command,
      unsigned int arg_count,
      const void* cmd_data));

  const char* GetCommandName(unsigned int command_id) const {
    return "";
  };

  // Sets the engine, to forward SetToken commands to it.
  void set_engine(CommandBufferEngine *engine) { engine_ = engine; }

  // Forwards the SetToken commands to the engine.
  void SetToken(unsigned int command,
                unsigned int arg_count,
                const void* _args) {
    DCHECK(engine_);
    DCHECK_EQ(1u, command);
    DCHECK_EQ(1u, arg_count);
    const CommandBufferEntry* args =
        static_cast<const CommandBufferEntry*>(_args);
    engine_->set_token(args[0].value_uint32);
  }
 private:
  CommandBufferEngine *engine_;
};

}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_SERVICE_CROSS_MOCKS_H_
