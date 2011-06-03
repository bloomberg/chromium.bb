// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread.h"
#include "base/time.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/mocks.h"

namespace gpu {

AsyncAPIMock::AsyncAPIMock() {
  testing::DefaultValue<error::Error>::Set(
      error::kNoError);
}

AsyncAPIMock::~AsyncAPIMock() {}

void AsyncAPIMock::SetToken(unsigned int command,
                            unsigned int arg_count,
                            const void* _args) {
  DCHECK(engine_);
  DCHECK_EQ(1u, command);
  DCHECK_EQ(1u, arg_count);
  const cmd::SetToken* args =
      static_cast<const cmd::SetToken*>(_args);
  engine_->set_token(args->token);
}

SpecializedDoCommandAsyncAPIMock::SpecializedDoCommandAsyncAPIMock() {}

SpecializedDoCommandAsyncAPIMock::~SpecializedDoCommandAsyncAPIMock() {}

error::Error SpecializedDoCommandAsyncAPIMock::DoCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  if (command == kTestQuantumCommand) {
    // Surpass the GpuScheduler scheduling quantum.
    base::TimeTicks start_time = base::TimeTicks::Now();
    while ((base::TimeTicks::Now() - start_time).InMicroseconds() <
           GpuScheduler::kMinimumSchedulerQuantumMicros) {
      base::PlatformThread::Sleep(1);
    }
  }
  return AsyncAPIMock::DoCommand(command, arg_count, cmd_data);
}

namespace gles2 {

MockShaderTranslator::MockShaderTranslator() {}

MockShaderTranslator::~MockShaderTranslator() {}

}  // namespace gles2
}  // namespace gpu
