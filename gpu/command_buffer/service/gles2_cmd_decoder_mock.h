// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the mock GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_

#include <vector>

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/size.h"

namespace gfx {
class GLContext;
class GLSurface;
}

namespace gpu {
class StreamTextureManager;

namespace gles2 {

class ContextGroup;
class MockGLES2Decoder : public GLES2Decoder {
 public:
  MockGLES2Decoder();
  virtual ~MockGLES2Decoder();

  MOCK_METHOD6(Initialize,
               bool(const scoped_refptr<gfx::GLSurface>& surface,
                    const scoped_refptr<gfx::GLContext>& context,
                    const gfx::Size& size,
                    const DisallowedFeatures& disallowed_features,
                    const char* allowed_extensions,
                    const std::vector<int32>& attribs));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD2(SetParent, bool(GLES2Decoder* parent, uint32 parent_texture_id));
  MOCK_METHOD1(ResizeOffscreenFrameBuffer, bool(const gfx::Size& size));
  MOCK_METHOD0(MakeCurrent, bool());
  MOCK_METHOD0(ReleaseCurrent, void());
  MOCK_METHOD1(GetServiceIdForTesting, uint32(uint32 client_id));
  MOCK_METHOD0(GetGLES2Util, GLES2Util*());
  MOCK_METHOD0(GetGLSurface, gfx::GLSurface*());
  MOCK_METHOD0(GetGLContext, gfx::GLContext*());
  MOCK_METHOD0(GetContextGroup, ContextGroup*());
  MOCK_METHOD1(SetResizeCallback, void(const base::Callback<void(gfx::Size)>&));
  MOCK_METHOD1(SetSwapBuffersCallback, void(const base::Closure&));
  MOCK_METHOD1(SetStreamTextureManager, void(StreamTextureManager*));
  MOCK_METHOD3(DoCommand, error::Error(unsigned int command,
                                       unsigned int arg_count,
                                       const void* cmd_data));
  MOCK_METHOD2(GetServiceTextureId, bool(uint32 client_texture_id,
                                         uint32* service_texture_id));
  MOCK_METHOD0(GetContextLostReason, error::ContextLostReason());
  MOCK_CONST_METHOD1(GetCommandName, const char*(unsigned int command_id));
  MOCK_METHOD8(ClearLevel, bool(
      unsigned service_id,
      unsigned bind_target,
      unsigned target,
      int level,
      unsigned format,
      unsigned type,
      int width,
      int height));

  DISALLOW_COPY_AND_ASSIGN(MockGLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_MOCK_H_
