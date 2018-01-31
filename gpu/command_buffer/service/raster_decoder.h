// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_

#include "base/macros.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class DecoderClient;

namespace gles2 {
class GLES2Util;
class Logger;
class Outputter;
}  // namespace gles2

namespace raster {

// This class implements the AsyncAPIInterface interface, decoding
// RasterInterface commands and calling GL.
class GPU_GLES2_EXPORT RasterDecoder : public DecoderContext,
                                       public CommonDecoder {
 public:
  static RasterDecoder* Create(DecoderClient* client,
                               CommandBufferServiceBase* command_buffer_service,
                               gles2::Outputter* outputter,
                               gles2::ContextGroup* group);

  ~RasterDecoder() override;

  // DecoderContext implementation.
  bool initialized() const override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void BeginDecoding() override;
  void EndDecoding() override;
  base::StringPiece GetLogPrefix() override;

  virtual gles2::GLES2Util* GetGLES2Util() = 0;
  virtual gles2::Logger* GetLogger() = 0;
  virtual void SetIgnoreCachedStateForTest(bool ignore) = 0;

  void set_initialized() { initialized_ = true; }

  // Set to true to call glGetError after every command.
  void set_debug(bool debug) { debug_ = debug; }
  bool debug() const { return debug_; }

  // Set to true to LOG every command.
  void set_log_commands(bool log_commands) { log_commands_ = log_commands; }
  bool log_commands() const { return log_commands_; }

 protected:
  RasterDecoder(CommandBufferServiceBase* command_buffer_service);

 private:
  bool initialized_;
  bool debug_;
  bool log_commands_;

  DISALLOW_COPY_AND_ASSIGN(RasterDecoder);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_H_
