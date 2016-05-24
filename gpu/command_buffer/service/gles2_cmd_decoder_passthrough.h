// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2DecoderPassthroughImpl class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_

namespace gpu {

namespace gles2 {

class ContextGroup;
class GLES2Decoder;

GLES2Decoder* CreateGLES2DecoderPassthroughImpl(ContextGroup* group);

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_H_
