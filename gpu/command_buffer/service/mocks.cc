// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  const CommandBufferEntry* args =
      static_cast<const CommandBufferEntry*>(_args);
  engine_->set_token(args[0].value_uint32);
}

namespace gles2 {

MockShaderTranslator::MockShaderTranslator() {}

MockShaderTranslator::~MockShaderTranslator() {}

}  // namespace gles2
}  // namespace gpu
