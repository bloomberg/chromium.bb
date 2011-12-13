// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_

#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "ui/gfx/size.h"

namespace gfx {
class GLContext;
class GLSurface;
}

namespace gpu {

class StreamTextureManager;

namespace gles2 {

class ContextGroup;
class GLES2Util;

struct DisallowedFeatures {
  DisallowedFeatures()
      : multisampling(false),
        driver_bug_workarounds(false) {
  }

  bool multisampling;
  bool driver_bug_workarounds;
};

// This class implements the AsyncAPIInterface interface, decoding GLES2
// commands and calling GL.
class GLES2Decoder : public CommonDecoder {
 public:
  typedef error::Error Error;

  // Creates a decoder.
  static GLES2Decoder* Create(ContextGroup* group);

  virtual ~GLES2Decoder();

  bool debug() const {
    return debug_;
  }

  void set_debug(bool debug) {
    debug_ = debug;
  }

  // Initializes the graphics context. Can create an offscreen
  // decoder with a frame buffer that can be referenced from the parent.
  // Takes ownership of GLContext.
  // Parameters:
  //  surface: the GL surface to render to.
  //  context: the GL context to render to.
  //  size: the size if the GL context is offscreen.
  //  allowed_extensions: A string in the same format as
  //      glGetString(GL_EXTENSIONS) that lists the extensions this context
  //      should allow. Passing NULL or "*" means allow all extensions.
  // Returns:
  //   true if successful.
  virtual bool Initialize(const scoped_refptr<gfx::GLSurface>& surface,
                          const scoped_refptr<gfx::GLContext>& context,
                          const gfx::Size& size,
                          const DisallowedFeatures& disallowed_features,
                          const char* allowed_extensions,
                          const std::vector<int32>& attribs) = 0;

  // Destroys the graphics context.
  virtual void Destroy() = 0;

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

  // Gets the associated GLSurface.
  virtual gfx::GLSurface* GetGLSurface() = 0;

  // Gets the associated GLContext.
  virtual gfx::GLContext* GetGLContext() = 0;

  // Gets the associated ContextGroup
  virtual ContextGroup* GetContextGroup() = 0;

  // Sets a callback which is called when a glResizeCHROMIUM command
  // is processed.
  virtual void SetResizeCallback(
      const base::Callback<void(gfx::Size)>& callback) = 0;

  // Sets a callback which is called when a SwapBuffers command is processed.
  virtual void SetSwapBuffersCallback(const base::Closure& callback) = 0;

  virtual void SetStreamTextureManager(StreamTextureManager* manager) = 0;

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
      int height) = 0;

 protected:
  GLES2Decoder();

 private:
  bool debug_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
