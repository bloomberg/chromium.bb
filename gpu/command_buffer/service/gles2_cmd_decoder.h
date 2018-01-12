// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/gpu_gles2_export.h"

namespace gl {
class GLSurface;
}

namespace gfx {
class Size;
}

namespace gpu {

class DecoderClient;
struct Mailbox;
class ServiceTransferCache;

namespace gles2 {

class ContextGroup;
class FeatureInfo;
class FramebufferManager;
class GLES2Util;
class ImageManager;
class Logger;
class Outputter;
class ShaderTranslatorInterface;
class Texture;
class TransformFeedbackManager;
class VertexArrayManager;

struct DisallowedFeatures {
  DisallowedFeatures() = default;

  void AllowExtensions() {
    chromium_color_buffer_float_rgba = false;
    chromium_color_buffer_float_rgb = false;
    ext_color_buffer_float = false;
    ext_color_buffer_half_float = false;
    oes_texture_float_linear = false;
    oes_texture_half_float_linear = false;
  }

  bool gpu_memory_manager = false;
  bool npot_support = false;
  bool chromium_color_buffer_float_rgba = false;
  bool chromium_color_buffer_float_rgb = false;
  bool ext_color_buffer_float = false;
  bool ext_color_buffer_half_float = false;
  bool oes_texture_float_linear = false;
  bool oes_texture_half_float_linear = false;
};

// This class implements the DecoderContext interface, decoding GLES2
// commands and calling GL.
class GPU_GLES2_EXPORT GLES2Decoder : public CommonDecoder,
                                      public DecoderContext {
 public:
  typedef error::Error Error;

  // The default stencil mask, which has all bits set.  This really should be a
  // GLuint, but we can't #include gl_bindings.h in this file without causing
  // macro redefinitions.
  static const unsigned int kDefaultStencilMask;

  // Creates a decoder.
  static GLES2Decoder* Create(DecoderClient* client,
                              CommandBufferServiceBase* command_buffer_service,
                              Outputter* outputter,
                              ContextGroup* group);

  ~GLES2Decoder() override;

  // DecoderContext implementation.
  bool initialized() const override;
  TextureBase* GetTextureBase(uint32_t client_id) override;
  void BeginDecoding() override;
  void EndDecoding() override;
  base::StringPiece GetLogPrefix() override;

  void set_initialized() {
    initialized_ = true;
  }

  bool debug() const {
    return debug_;
  }

  // Set to true to call glGetError after every command.
  void set_debug(bool debug) {
    debug_ = debug;
  }

  bool log_commands() const {
    return log_commands_;
  }

  // Set to true to LOG every command.
  void set_log_commands(bool log_commands) {
    log_commands_ = log_commands;
  }

  Outputter* outputter() const { return outputter_; }

  // Set the surface associated with the default FBO.
  virtual void SetSurface(const scoped_refptr<gl::GLSurface>& surface) = 0;
  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  virtual void ReleaseSurface() = 0;

  virtual void TakeFrontBuffer(const Mailbox& mailbox) = 0;
  virtual void ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) = 0;

  // Resize an offscreen frame buffer.
  virtual bool ResizeOffscreenFramebuffer(const gfx::Size& size) = 0;

  // Gets the GLES2 Util which holds info.
  virtual GLES2Util* GetGLES2Util() = 0;

  virtual const FeatureInfo* GetFeatureInfo() const = 0;

  // Restore States.
  virtual void RestoreGlobalState() const = 0;
  virtual void RestoreDeviceWindowRectangles() const = 0;

  virtual void ClearAllAttributes() const = 0;
  virtual void RestoreAllAttributes() const = 0;

  virtual void SetIgnoreCachedStateForTest(bool ignore) = 0;
  virtual void SetForceShaderNameHashingForTest(bool force) = 0;
  virtual uint32_t GetAndClearBackbufferClearBitsForTest();
  virtual size_t GetSavedBackTextureCountForTest() = 0;
  virtual size_t GetCreatedBackTextureCountForTest() = 0;

  // Gets the FramebufferManager for this context.
  virtual FramebufferManager* GetFramebufferManager() = 0;

  // Gets the TransformFeedbackManager for this context.
  virtual TransformFeedbackManager* GetTransformFeedbackManager() = 0;

  // Gets the VertexArrayManager for this context.
  virtual VertexArrayManager* GetVertexArrayManager() = 0;

  // Gets the ImageManager for this context.
  virtual ImageManager* GetImageManagerForTest() = 0;

  // Gets the ServiceTransferCache for this context.
  virtual ServiceTransferCache* GetTransferCacheForTest() = 0;

  // Get the service texture ID corresponding to a client texture ID.
  // If no such record is found then return false.
  virtual bool GetServiceTextureId(uint32_t client_texture_id,
                                   uint32_t* service_texture_id);

  // Clears a level sub area of a 2D texture.
  // Returns false if a GL error should be generated.
  virtual bool ClearLevel(Texture* texture,
                          unsigned target,
                          int level,
                          unsigned format,
                          unsigned type,
                          int xoffset,
                          int yoffset,
                          int width,
                          int height) = 0;

  // Clears a level sub area of a compressed 2D texture.
  // Returns false if a GL error should be generated.
  virtual bool ClearCompressedTextureLevel(Texture* texture,
                                           unsigned target,
                                           int level,
                                           unsigned format,
                                           int width,
                                           int height) = 0;

  // Indicates whether a given internal format is one for a compressed
  // texture.
  virtual bool IsCompressedTextureFormat(unsigned format) = 0;

  // Clears a level of a 3D texture.
  // Returns false if a GL error should be generated.
  virtual bool ClearLevel3D(Texture* texture,
                            unsigned target,
                            int level,
                            unsigned format,
                            unsigned type,
                            int width,
                            int height,
                            int depth) = 0;

  virtual void WaitForReadPixels(base::Closure callback) = 0;

  virtual Logger* GetLogger() = 0;

  virtual scoped_refptr<ShaderTranslatorInterface> GetTranslator(
      unsigned int type) = 0;

 protected:
  GLES2Decoder(CommandBufferServiceBase* command_buffer_service,
               Outputter* outputter);

 private:
  bool initialized_ = false;
  bool debug_ = false;
  bool log_commands_ = false;
  Outputter* outputter_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
