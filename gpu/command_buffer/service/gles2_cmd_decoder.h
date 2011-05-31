// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GLES2Decoder class.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_old.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "ui/gfx/size.h"

namespace gfx {
class GLContext;
class GLSurface;
}

namespace gpu {

namespace gles2 {

class ContextGroup;
class GLES2Util;

struct DisallowedExtensions {
  DisallowedExtensions() : multisampling(false) {}

  bool multisampling;
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
  //  parent: the GLES2 decoder that can access this decoder's front buffer
  //      through a texture ID in its namespace.
  //  parent_client_texture_id: the texture ID of the front buffer in the
  //      parent's namespace.
  // Returns:
  //   true if successful.
  virtual bool Initialize(gfx::GLSurface* surface,
                          gfx::GLContext* context,
                          const gfx::Size& size,
                          const DisallowedExtensions& disallowed_extensions,
                          const char* allowed_extensions,
                          const std::vector<int32>& attribs,
                          GLES2Decoder* parent,
                          uint32 parent_client_texture_id) = 0;

  // Destroys the graphics context.
  virtual void Destroy() = 0;

  // Resize an offscreen frame buffer.
  virtual void ResizeOffscreenFrameBuffer(const gfx::Size& size) = 0;

  // Force the offscreen frame buffer's size to be updated. This
  // usually occurs lazily, during SwapBuffers, but on some platforms
  // (Mac OS X in particular) it must be done immediately.
  virtual bool UpdateOffscreenFrameBufferSize() = 0;

  // Make this decoder's GL context current.
  virtual bool MakeCurrent() = 0;

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
  virtual void SetResizeCallback(Callback1<gfx::Size>::Type* callback) = 0;

  // Sets a callback which is called when a SwapBuffers command is processed.
  virtual void SetSwapBuffersCallback(Callback0::Type* callback) = 0;

  // Sets a callback which is called after a Set/WaitLatch command is processed.
  // The bool parameter will be true for SetLatch, and false for a WaitLatch
  // that is blocked. An unblocked WaitLatch will not trigger a callback.
  virtual void SetLatchCallback(const base::Callback<void(bool)>& callback) = 0;

  // Get the service texture ID corresponding to a client texture ID.
  // If no such record is found then return false.
  virtual bool GetServiceTextureId(uint32 client_texture_id,
                                   uint32* service_texture_id);

 protected:
  GLES2Decoder();

 private:
  bool debug_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Decoder);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_H_
