// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This API is consistent with other OpenGL setup APIs like window's WGL
// and pepper's PGL. This API is used to manage OpenGL ContentGLContexts in the
// Chrome renderer process in a way that is consistent with other platforms.

#ifndef CONTENT_COMMON_GPU_CLIENT_RENDERER_GL_CONTEXT_H_
#define CONTENT_COMMON_GPU_CLIENT_RENDERER_GL_CONTEXT_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class GpuChannelHost;
class CommandBufferProxy;
class GURL;
struct GpuMemoryAllocationForRenderer;

namespace gpu {
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}
}

class ContentGLContext : public base::SupportsWeakPtr<ContentGLContext>,
                         public base::NonThreadSafe {
 public:
  // These are the same error codes as used by EGL.
  enum Error {
    SUCCESS               = 0x3000,
    BAD_ATTRIBUTE         = 0x3004,
    CONTEXT_LOST          = 0x300E
  };

  // ContentGLContext configuration attributes. Those in the 16-bit range are
  // the same as used by EGL. Those outside the 16-bit range are unique to
  // Chromium. Attributes are matched using a closest fit algorithm.
  enum Attribute {
    ALPHA_SIZE                = 0x3021,
    BLUE_SIZE                 = 0x3022,
    GREEN_SIZE                = 0x3023,
    RED_SIZE                  = 0x3024,
    DEPTH_SIZE                = 0x3025,
    STENCIL_SIZE              = 0x3026,
    SAMPLES                   = 0x3031,
    SAMPLE_BUFFERS            = 0x3032,
    HEIGHT                    = 0x3056,
    WIDTH                     = 0x3057,
    NONE                      = 0x3038,  // Attrib list = terminator
    SHARE_RESOURCES           = 0x10000,
    BIND_GENERATES_RESOURCES  = 0x10001
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

  ~ContentGLContext();

  // Create a ContentGLContext that renders directly to a view. The view and
  // the associated window must not be destroyed until the returned
  // ContentGLContext has been destroyed, otherwise the GPU process might
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
  static ContentGLContext* CreateViewContext(
      GpuChannelHost* channel,
      int32 surface_id,
      ContentGLContext* share_group,
      const char* allowed_extensions,
      const int32* attrib_list,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Create a ContentGLContext that renders to an offscreen frame buffer. If
  // parent is not NULL, that ContentGLContext can access a copy of the created
  // ContentGLContext's frame buffer that is updated every time SwapBuffers is
  // called. It is not as general as shared ContentGLContexts in other
  // implementations of OpenGL. If parent is not NULL, it must be used on the
  // same thread as the parent. A child ContentGLContext may not outlive its
  // parent.  attrib_list must be NULL or a NONE-terminated list of
  // attribute/value pairs.
  static ContentGLContext* CreateOffscreenContext(
      GpuChannelHost* channel,
      const gfx::Size& size,
      ContentGLContext* share_group,
      const char* allowed_extensions,
      const int32* attrib_list,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Sets the parent context. If any parent textures have been created for
  // another parent, it is important to delete them before changing the parent.
  bool SetParent(ContentGLContext* parent);

  // For an offscreen frame buffer ContentGLContext, return the texture ID with
  // respect to the parent ContentGLContext. Returns zero if ContentGLContext
  // does not have a parent.
  uint32 GetParentTextureId();

  // Create a new texture in the parent's ContentGLContext.  Returns zero if
  // ContentGLContext does not have a parent.
  uint32 CreateParentTexture(const gfx::Size& size);

  // Deletes a texture in the parent's ContentGLContext.
  void DeleteParentTexture(uint32 texture);

  void SetContextLostCallback(
      const base::Callback<void(ContextLostReason)>& callback);

  // Set the current ContentGLContext for the calling thread.
  static bool MakeCurrent(ContentGLContext* context);

  // For a view ContentGLContext, display everything that has been rendered
  // since the last call. For an offscreen ContentGLContext, resolve everything
  // that has been rendered since the last call to a copy that can be accessed
  // by the parent ContentGLContext.
  bool SwapBuffers();

  // Run the task once the channel has been flushed. Takes care of deleting the
  // task whether the echo succeeds or not.
  bool Echo(const base::Closure& task);

  // Sends an IPC message with the new state of surface visibility
  bool SetSurfaceVisible(bool visibility);

  bool DiscardBackbuffer();
  bool EnsureBackbuffer();

  // Register a callback to invoke whenever we recieve a new memory allocation.
  void SetMemoryAllocationChangedCallback(
      const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
          callback);

  // Allows direct access to the GLES2 implementation so a ContentGLContext
  // can be used without making it current.
  gpu::gles2::GLES2Implementation* GetImplementation();

  // Return the current error.
  Error GetError();

  // Return true if GPU process reported ContentGLContext lost or there was a
  // problem communicating with the GPU process.
  bool IsCommandBufferContextLost();

  CommandBufferProxy* GetCommandBufferProxy();

  // The following 3 IDs let one uniquely identify this context.
  // Gets the GPU process ID for this context.
  int GetGPUProcessID();
  // Gets the channel ID for this context.
  int GetChannelID();
  // Gets the context ID (relative to the channel).
  int GetContextID();

 private:
  explicit ContentGLContext(GpuChannelHost* channel);

  bool Initialize(bool onscreen,
                  int32 surface_id,
                  const gfx::Size& size,
                  ContentGLContext* share_group,
                  const char* allowed_extensions,
                  const int32* attrib_list,
                  const GURL& active_url,
                  gfx::GpuPreference gpu_preference);
  void Destroy();

  void OnContextLost();

  scoped_refptr<GpuChannelHost> channel_;
  base::WeakPtr<ContentGLContext> parent_;
  base::Callback<void(ContextLostReason)> context_lost_callback_;
  uint32 parent_texture_id_;
  CommandBufferProxy* command_buffer_;
  gpu::gles2::GLES2CmdHelper* gles2_helper_;
  gpu::TransferBuffer* transfer_buffer_;
  gpu::gles2::GLES2Implementation* gles2_implementation_;
  Error last_error_;
  int frame_number_;

  DISALLOW_COPY_AND_ASSIGN(ContentGLContext);
};

#endif  // CONTENT_COMMON_GPU_CLIENT_RENDERER_GL_CONTEXT_H_
