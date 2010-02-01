// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains definitions for mock objects, used for testing.

// TODO: This file "manually" defines some mock objects. Using gMock
// would be definitely preferable, unfortunately it doesn't work on Windows
// yet.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MOCKS_H_
#define GPU_COMMAND_BUFFER_SERVICE_MOCKS_H_

#include <vector>
#include "testing/gmock/include/gmock/gmock.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"

namespace gpu {

// Mocks an AsyncAPIInterface, using GMock.
class AsyncAPIMock : public AsyncAPIInterface {
 public:
  AsyncAPIMock() {
    testing::DefaultValue<error::Error>::Set(
        error::kNoError);
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

  MOCK_METHOD3(DoCommand, error::Error(
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

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MOCKS_H_
