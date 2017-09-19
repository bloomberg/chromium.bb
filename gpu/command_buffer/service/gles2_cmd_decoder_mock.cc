// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

MockGLES2Decoder::MockGLES2Decoder(
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter)
    : GLES2Decoder(command_buffer_service, outputter), weak_ptr_factory_(this) {
  ON_CALL(*this, MakeCurrent())
      .WillByDefault(testing::Return(true));
}

MockGLES2Decoder::~MockGLES2Decoder() {}

base::WeakPtr<GLES2Decoder> MockGLES2Decoder::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace gles2
}  // namespace gpu
