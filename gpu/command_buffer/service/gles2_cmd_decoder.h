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
#include "gpu/gpu_export.h"

namespace gl {
class GLContext;
class GLSurface;
class GLImage;
}

namespace gfx {
class Size;
}

namespace gpu {

struct Mailbox;
struct SyncToken;

namespace gles2 {

class ContextGroup;
class ErrorState;
class FeatureInfo;
class FramebufferManager;
class GLES2Util;
class ImageManager;
class Logger;
class Outputter;
class QueryManager;
class ShaderTranslatorInterface;
class Texture;
class TextureBase;
class TransformFeedbackManager;
class VertexArrayManager;
struct ContextCreationAttribHelper;
struct ContextState;

struct DisallowedFeatures {
  DisallowedFeatures() {}

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

class GPU_EXPORT GLES2DecoderClient {
 public:
  virtual ~GLES2DecoderClient() {}

  // Prints a message (error/warning) to the console.
  virtual void OnConsoleMessage(int32_t id, const std::string& message) = 0;

  // Cache a newly linked shader.
  virtual void CacheShader(const std::string& key,
                           const std::string& shader) = 0;

  // Called when the decoder releases a fence sync. Allows the client to
  // reschedule waiting decoders.
  virtual void OnFenceSyncRelease(uint64_t release) = 0;

  // Called when the decoder needs to wait on a sync token. If the wait is valid
  // (fence sync is not released yet), the client must unschedule the command
  // buffer and return true. The client is responsible for rescheduling the
  // command buffer when the fence is released.  If the wait is a noop (fence is
  // already released) or invalid, the client must leave the command buffer
  // scheduled, and return false.
  virtual bool OnWaitSyncToken(const gpu::SyncToken&) = 0;

  // Called when the decoder needs to be descheduled while waiting for a fence
  // completion. The client is responsible for descheduling the command buffer
  // before returning, and then calling PerformPollingWork periodically to test
  // for the fence completion and possibly reschedule.
  virtual void OnDescheduleUntilFinished() = 0;

  // Called from PerformPollingWork when the decoder needs to be rescheduled
  // because the fence completed.
  virtual void OnRescheduleAfterFinished() = 0;
};

// This class implements the AsyncAPIInterface interface, decoding GLES2
// commands and calling GL.
class GPU_EXPORT GLES2Decoder : public CommonDecoder, public AsyncAPIInterface {
 public:
  typedef error::Error Error;

  // The default stencil mask, which has all bits set.  This really should be a
  // GLuint, but we can't #include gl_bindings.h in this file without causing
  // macro redefinitions.
  static const unsigned int kDefaultStencilMask;

  // Creates a decoder.
  static GLES2Decoder* Create(GLES2DecoderClient* client,
                              CommandBufferServiceBase* command_buffer_service,
                              Outputter* outputter,
                              ContextGroup* group);

  ~GLES2Decoder() override;

  bool initialized() const {
    return initialized_;
  }

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

  virtual base::WeakPtr<GLES2Decoder> AsWeakPtr() = 0;

  // Initializes the graphics context. Can create an offscreen
  // decoder with a frame buffer that can be referenced from the parent.
  // Takes ownership of GLContext.
  // Parameters:
  //  surface: the GL surface to render to.
  //  context: the GL context to render to.
  //  offscreen: whether to make the context offscreen or not. When FBO 0 is
  //      bound, offscreen contexts render to an internal buffer, onscreen ones
  //      to the surface.
  //  offscreen_size: the size if the GL context is offscreen.
  // Returns:
  //   true if successful.
  virtual gpu::ContextResult Initialize(
      const scoped_refptr<gl::GLSurface>& surface,
      const scoped_refptr<gl::GLContext>& context,
      bool offscreen,
      const DisallowedFeatures& disallowed_features,
      const ContextCreationAttribHelper& attrib_helper) = 0;

  // Destroys the graphics context.
  virtual void Destroy(bool have_context) = 0;

  // Set the surface associated with the default FBO.
  virtual void SetSurface(const scoped_refptr<gl::GLSurface>& surface) = 0;
  // Releases the surface associated with the GL context.
  // The decoder should not be used until a new surface is set.
  virtual void ReleaseSurface() = 0;

  virtual void TakeFrontBuffer(const Mailbox& mailbox) = 0;
  virtual void ReturnFrontBuffer(const Mailbox& mailbox, bool is_lost) = 0;

  // Resize an offscreen frame buffer.
  virtual bool ResizeOffscreenFramebuffer(const gfx::Size& size) = 0;

  // Make this decoder's GL context current.
  virtual bool MakeCurrent() = 0;

  // Gets the GLES2 Util which holds info.
  virtual GLES2Util* GetGLES2Util() = 0;

  // Gets the associated GLContext.
  virtual gl::GLContext* GetGLContext() = 0;

  // Gets the associated ContextGroup
  virtual ContextGroup* GetContextGroup() = 0;
  virtual const FeatureInfo* GetFeatureInfo() const = 0;

  virtual Capabilities GetCapabilities() = 0;

  // Restores all of the decoder GL state.
  virtual void RestoreState(const ContextState* prev_state) = 0;

  // Restore States.
  virtual void RestoreActiveTexture() const = 0;
  virtual void RestoreAllTextureUnitAndSamplerBindings(
      const ContextState* prev_state) const = 0;
  virtual void RestoreActiveTextureUnitBinding(unsigned int target) const = 0;
  virtual void RestoreBufferBinding(unsigned int target) = 0;
  virtual void RestoreBufferBindings() const = 0;
  virtual void RestoreFramebufferBindings() const = 0;
  virtual void RestoreRenderbufferBindings() = 0;
  virtual void RestoreGlobalState() const = 0;
  virtual void RestoreProgramBindings() const = 0;
  virtual void RestoreTextureState(unsigned service_id) const = 0;
  virtual void RestoreTextureUnitBindings(unsigned unit) const = 0;
  virtual void RestoreVertexAttribArray(unsigned index) = 0;
  virtual void RestoreAllExternalTextureBindingsIfNeeded() = 0;

  virtual void ClearAllAttributes() const = 0;
  virtual void RestoreAllAttributes() const = 0;

  virtual void SetIgnoreCachedStateForTest(bool ignore) = 0;
  virtual void SetForceShaderNameHashingForTest(bool force) = 0;
  virtual uint32_t GetAndClearBackbufferClearBitsForTest();
  virtual size_t GetSavedBackTextureCountForTest() = 0;
  virtual size_t GetCreatedBackTextureCountForTest() = 0;

  // Gets the QueryManager for this context.
  virtual QueryManager* GetQueryManager() = 0;

  // Gets the FramebufferManager for this context.
  virtual FramebufferManager* GetFramebufferManager() = 0;

  // Gets the TransformFeedbackManager for this context.
  virtual TransformFeedbackManager* GetTransformFeedbackManager() = 0;

  // Gets the VertexArrayManager for this context.
  virtual VertexArrayManager* GetVertexArrayManager() = 0;

  // Gets the ImageManager for this context.
  virtual ImageManager* GetImageManagerForTest() = 0;

  // Returns false if there are no pending queries.
  virtual bool HasPendingQueries() const = 0;

  // Process any pending queries.
  virtual void ProcessPendingQueries(bool did_finish) = 0;

  // Returns false if there is no idle work to be made.
  virtual bool HasMoreIdleWork() const = 0;

  // Perform any idle work that needs to be made.
  virtual void PerformIdleWork() = 0;

  // Whether there is state that needs to be regularly polled.
  virtual bool HasPollingWork() const = 0;

  // Perform necessary polling.
  virtual void PerformPollingWork() = 0;

  // Get the service texture ID corresponding to a client texture ID.
  // If no such record is found then return false.
  virtual bool GetServiceTextureId(uint32_t client_texture_id,
                                   uint32_t* service_texture_id);

  // Gets the texture object associated with the client ID.  null is returned on
  // failure or if the texture has not been bound yet.
  virtual TextureBase* GetTextureBase(uint32_t client_id);

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

  virtual ErrorState* GetErrorState() = 0;

  virtual void WaitForReadPixels(base::Closure callback) = 0;

  // Returns true if the context was lost either by GL_ARB_robustness, forced
  // context loss or command buffer parse error.
  virtual bool WasContextLost() const = 0;

  // Returns true if the context was lost specifically by GL_ARB_robustness.
  virtual bool WasContextLostByRobustnessExtension() const = 0;

  // Lose this context.
  virtual void MarkContextLost(error::ContextLostReason reason) = 0;

  // Updates context lost state and returns true if lost. Most callers can use
  // WasContextLost() as the GLES2Decoder will update the state internally. But
  // if making GL calls directly, to the context then this state would not be
  // updated and the caller can use this to determine if their calls failed due
  // to context loss.
  virtual bool CheckResetStatus() = 0;

  virtual Logger* GetLogger() = 0;

  void BeginDecoding() override;
  void EndDecoding() override;

  virtual const ContextState* GetContextState() = 0;
  virtual scoped_refptr<ShaderTranslatorInterface> GetTranslator(
      unsigned int type) = 0;

  virtual void BindImage(uint32_t client_texture_id,
                         uint32_t texture_target,
                         gl::GLImage* image,
                         bool can_bind_to_sampler) = 0;

 protected:
  GLES2Decoder(CommandBufferServiceBase* command_buffer_service,
               Outputter* outputter);

  base::StringPiece GetLogPrefix() override;

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
