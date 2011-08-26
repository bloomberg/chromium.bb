// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This API is consistent with other OpenGL setup APIs like window's WGL
// and pepper's PGL. This API is used to manage OpenGL RendererGLContexts in the
// Chrome renderer process in a way that is consistent with other platforms.

#ifndef CONTENT_RENDERER_GPU_RENDERER_GL_CONTEXT_H_
#define CONTENT_RENDERER_GPU_RENDERER_GL_CONTEXT_H_
#pragma once

#include "base/callback_old.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class GpuChannelHost;
class MessageLoop;
class CommandBufferProxy;
class GURL;
class TransportTextureHost;

namespace gpu {
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}
}

class RendererGLContext : public base::SupportsWeakPtr<RendererGLContext> {
 public:
  // These are the same error codes as used by EGL.
  enum Error {
    SUCCESS             = 0x3000,
    NOT_INITIALIZED     = 0x3001,
    BAD_ATTRIBUTE       = 0x3004,
    BAD_RendererGLContext         = 0x3006,
    CONTEXT_LOST        = 0x300E
  };

  // RendererGLContext configuration attributes. These are the same as used by
  // EGL. Attributes are matched using a closest fit algorithm.
  enum Attribute {
    ALPHA_SIZE     = 0x3021,
    BLUE_SIZE      = 0x3022,
    GREEN_SIZE     = 0x3023,
    RED_SIZE       = 0x3024,
    DEPTH_SIZE     = 0x3025,
    STENCIL_SIZE   = 0x3026,
    SAMPLES        = 0x3031,
    SAMPLE_BUFFERS = 0x3032,
    HEIGHT         = 0x3056,
    WIDTH          = 0x3057,
    NONE           = 0x3038  // Attrib list = terminator
  };

  // Reasons that a lost context might have been provoked.
  enum ContextLostReason {
    // This context definitely provoked the loss of context.
    kGuilty,

    // This context definitely did not provoke the loss of context.
    kInnocent,

    // It is unknown whether this context provoked the loss of context.
    kUnknown
  };

  // Initialize the library. This must have completed before any other
  // functions are invoked.
  static bool Initialize();

  // Terminate the library. This must be called after any other functions
  // have completed.
  static bool Terminate();

  ~RendererGLContext();

  // Create a RendererGLContext that renders directly to a view. The view and
  // the associated window must not be destroyed until the returned
  // RendererGLContext has been destroyed, otherwise the GPU process might
  // attempt to render to an invalid window handle.
  //
  // NOTE: on Mac OS X, this entry point is only used to set up the
  // accelerated compositor's output. On this platform, we actually pass
  // a gfx::PluginWindowHandle in place of the gfx::NativeViewId,
  // because the facility to allocate a fake PluginWindowHandle is
  // already in place. We could add more entry points and messages to
  // allocate both fake PluginWindowHandles and NativeViewIds and map
  // from fake NativeViewIds to PluginWindowHandles, but this seems like
  // unnecessary complexity at the moment.
  //
  // The render_view_id is currently also only used on Mac OS X.
  // TODO(kbr): clean up the arguments to this function and make them
  // more cross-platform.
  static RendererGLContext* CreateViewContext(
      GpuChannelHost* channel,
      int render_view_id,
      bool share_resources,
      RendererGLContext* share_group,
      const char* allowed_extensions,
      const int32* attrib_list,
      const GURL& active_arl);

  // Create a RendererGLContext that renders to an offscreen frame buffer. If
  // parent is not NULL, that RendererGLContext can access a copy of the created
  // RendererGLContext's frame buffer that is updated every time SwapBuffers is
  // called. It is not as general as shared RendererGLContexts in other
  // implementations of OpenGL. If parent is not NULL, it must be used on the
  // same thread as the parent. A child RendererGLContext may not outlive its
  // parent.  attrib_list must be NULL or a NONE-terminated list of
  // attribute/value pairs.
  static RendererGLContext* CreateOffscreenContext(
      GpuChannelHost* channel,
      const gfx::Size& size,
      bool share_resources,
      RendererGLContext* share_group,
      const char* allowed_extensions,
      const int32* attrib_list,
      const GURL& active_url);

  // Sets the parent context. If any parent textures have been created for
  // another parent, it is important to delete them before changing the parent.
  bool SetParent(RendererGLContext* parent);

  // For an offscreen frame buffer RendererGLContext, return the texture ID with
  // respect to the parent RendererGLContext. Returns zero if RendererGLContext
  // does not have a parent.
  uint32 GetParentTextureId();

  // Create a new texture in the parent's RendererGLContext.  Returns zero if
  // RendererGLContext does not have a parent.
  uint32 CreateParentTexture(const gfx::Size& size);

  // Deletes a texture in the parent's RendererGLContext.
  void DeleteParentTexture(uint32 texture);

  // Provides a callback that will be invoked when SwapBuffers has completed
  // service side.
  void SetSwapBuffersCallback(Callback0::Type* callback);

  void SetContextLostCallback(Callback1<ContextLostReason>::Type* callback);

  // Set the current RendererGLContext for the calling thread.
  static bool MakeCurrent(RendererGLContext* context);

  // For a view RendererGLContext, display everything that has been rendered
  // since the last call. For an offscreen RendererGLContext, resolve everything
  // that has been rendered since the last call to a copy that can be accessed
  // by the parent RendererGLContext.
  bool SwapBuffers();

  // Create a TransportTextureHost object associated with the context.
  scoped_refptr<TransportTextureHost> CreateTransportTextureHost();

  // TODO(gman): Remove this
  void DisableShaderTranslation();

  // Allows direct access to the GLES2 implementation so a RendererGLContext
  // can be used without making it current.
  gpu::gles2::GLES2Implementation* GetImplementation();

  // Return the current error.
  Error GetError();

  // Return true if GPU process reported RendererGLContext lost or there was a
  // problem communicating with the GPU process.
  bool IsCommandBufferContextLost();

  CommandBufferProxy* GetCommandBufferProxy();

 private:
  explicit RendererGLContext(GpuChannelHost* channel);

  bool Initialize(bool onscreen,
                  int render_view_id,
                  const gfx::Size& size,
                  bool share_resources,
                  bool bind_generates_resource,
                  RendererGLContext* share_group,
                  const char* allowed_extensions,
                  const int32* attrib_list,
                  const GURL& active_url);
  void Destroy();

  void OnSwapBuffers();
  void OnContextLost();

  scoped_refptr<GpuChannelHost> channel_;
  base::WeakPtr<RendererGLContext> parent_;
  scoped_ptr<Callback0::Type> swap_buffers_callback_;
  scoped_ptr<Callback1<ContextLostReason>::Type> context_lost_callback_;
  uint32 parent_texture_id_;
  CommandBufferProxy* command_buffer_;
  gpu::gles2::GLES2CmdHelper* gles2_helper_;
  int32 transfer_buffer_id_;
  gpu::gles2::GLES2Implementation* gles2_implementation_;
  Error last_error_;
  int frame_number_;
#ifndef NDEBUG
  // Used to assert that this object is used on a single thread.
  MessageLoop* message_loop_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RendererGLContext);
};

#endif  // CONTENT_RENDERER_GPU_RENDERER_GL_CONTEXT_H_
