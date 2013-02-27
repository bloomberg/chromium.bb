// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_context.h"

namespace gfx {
class GLContext;
class GLSurface;
class AsyncPixelTransferDelegate;
}

namespace gpu {

class StreamTextureManager;

namespace gles2 {

class ContextGroup;
class GLES2Util;
class QueryManager;
class VertexArrayManager;

struct DisallowedFeatures {
  DisallowedFeatures()
      : multisampling(false),
        swap_buffer_complete_callback(false),
        gpu_memory_manager(false) {
  }

  bool multisampling;
  bool swap_buffer_complete_callback;
  bool gpu_memory_manager;
};

// This class implements the AsyncAPIInterface interface, decoding GLES2
// commands and calling GL.
class GPU_EXPORT GLES2Decoder : public base::SupportsWeakPtr<GLES2Decoder>,
                                public CommonDecoder {
 public:
  typedef error::Error Error;
  typedef base::Callback<void(int32 id, const std::string& msg)> MsgCallback;
  typedef base::Callback<bool(uint32 id)> WaitSyncPointCallback;

  // Creates a decoder.
  static GLES2Decoder* Create(ContextGroup* group);

  virtual ~GLES2Decoder();

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

  bool log_synthesized_gl_errors() const {
    return log_synthesized_gl_errors_;
  }

  // Defaults to true. Set to false for the gpu_unittests as they
  // are explicitly checking errors are generated and so don't need the numerous
  // messages. Otherwise, chromium code that generates these errors likely has a
  // bug.
  void set_log_synthesized_gl_errors(bool enabled) {
    log_synthesized_gl_errors_ = enabled;
  }

  // Initializes the graphics context. Can create an offscreen
  // decoder with a frame buffer that can be referenced from the parent.
  // Takes ownership of GLContext.
  // Parameters:
  //  surface: the GL surface to render to.
  //  context: the GL context to render to.
  //  offscreen: whether to make the context offscreen or not. When FBO 0 is
  //      bound, offscreen contexts render to an internal buffer, onscreen ones
  //      to the surface.
  //  size: the size if the GL context is offscreen.
  //  allowed_extensions: A string in the same format as
  //      glGetString(GL_EXTENSIONS) that lists the extensions this context
  //      should allow. Passing NULL or "*" means allow all extensions.
  // Returns:
  //   true if successful.
  virtual bool Initialize(const scoped_refptr<gfx::GLSurface>& surface,
                          const scoped_refptr<gfx::GLContext>& context,
                          bool offscreen,
                          const gfx::Size& size,
                          const DisallowedFeatures& disallowed_features,
                          const char* allowed_extensions,
                          const std::vector<int32>& attribs) = 0;

  // Destroys the graphics context.
  virtual void Destroy(bool have_context) = 0;

  // Set the surface associated with the default FBO.
  virtual void SetSurface(const scoped_refptr<gfx::GLSurface>& surface) = 0;

  virtual bool SetParent(GLES2Decoder* parent_decoder,
                         uint32 parent_texture_id) = 0;

  // Resize an offscreen frame buffer.
  virtual bool ResizeOffscreenFrameBuffer(const gfx::Size& size) = 0;

  // Make this decoder's GL context current.
  virtual bool MakeCurrent() = 0;

  // Have the decoder release the context.
  virtual void ReleaseCurrent() = 0;

  // Gets the GLES2 Util which holds info.
  virtual GLES2Util* GetGLES2Util() = 0;

  // Gets the associated GLContext.
  virtual gfx::GLContext* GetGLContext() = 0;

  // Gets the associated ContextGroup
  virtual ContextGroup* GetContextGroup() = 0;

  // Gets the service id for any simulated backbuffer fbo.
  virtual void RestoreState() const = 0;

  // Restore States.
  virtual void RestoreActiveTexture() const = 0;
  virtual void RestoreAttribute(unsigned index) const = 0;
  virtual void RestoreBufferBindings() const = 0;
  virtual void RestoreFramebufferBindings() const = 0;
  virtual void RestoreGlobalState() const = 0;
  virtual void RestoreProgramBindings() const = 0;
  virtual void RestoreRenderbufferBindings() const = 0;
  virtual void RestoreTextureState(unsigned service_id) const = 0;
  virtual void RestoreTextureUnitBindings(unsigned unit) const = 0;

  // Gets the QueryManager for this context.
  virtual QueryManager* GetQueryManager() = 0;

  // Gets the VertexArrayManager for this context.
  virtual VertexArrayManager* GetVertexArrayManager() = 0;

  // Process any pending queries. Returns false if there are no pending queries.
  virtual bool ProcessPendingQueries() = 0;

  // Sets a callback which is called when a glResizeCHROMIUM command
  // is processed.
  virtual void SetResizeCallback(
      const base::Callback<void(gfx::Size)>& callback) = 0;

  virtual void SetStreamTextureManager(StreamTextureManager* manager) = 0;

  // Interface to performing async pixel transfers.
  virtual gfx::AsyncPixelTransferDelegate* GetAsyncPixelTransferDelegate() = 0;
  virtual void SetAsyncPixelTransferDelegate(
      gfx::AsyncPixelTransferDelegate* delegate) = 0;

  // Get the service texture ID corresponding to a client texture ID.
  // If no such record is found then return false.
  virtual bool GetServiceTextureId(uint32 client_texture_id,
                                   uint32* service_texture_id);

  // Provides detail about a lost context if one occurred.
  virtual error::ContextLostReason GetContextLostReason() = 0;

  // Clears a level of a texture
  // Returns false if a GL error should be generated.
  virtual bool ClearLevel(
      unsigned service_id,
      unsigned bind_target,
      unsigned target,
      int level,
      unsigned format,
      unsigned type,
      int width,
      int height,
      bool is_texture_immutable) = 0;

  // Gets the GL error for this context.
  virtual uint32 GetGLError() = 0;

  // A callback for messages from the decoder.
  virtual void SetMsgCallback(const MsgCallback& callback) = 0;

  // Sets the callback for waiting on a sync point. The callback returns the
  // scheduling status (i.e. true if the channel is still scheduled).
  virtual void SetWaitSyncPointCallback(
      const WaitSyncPointCallback& callback) = 0;

  virtual uint32 GetTextureUploadCount() = 0;
  virtual base::TimeDelta GetTotalTextureUploadTime() = 0;
  virtual base::TimeDelta GetTotalProcessingCommandsTime() = 0;
  virtual void AddProcessingCommandsTime(base::TimeDelta) = 0;

  // Returns true if the context was just lost due to e.g. GL_ARB_robustness.
  virtual bool WasContextLost() = 0;

  // Lose this context.
  virtual void LoseContext(uint32 reset_status) = 0;

  static bool IsAngle();

  // Used for testing only
  static void set_testing_force_is_angle(bool force);

 protected:
  GLES2Decoder();

 private:
  bool debug_;
  bool log_commands_;
  bool log_synthesized_gl_errors_;
  static bool testing_force_is_angle_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
