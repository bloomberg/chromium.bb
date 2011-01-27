// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This API is consistent with other OpenGL setup APIs like window's WGL
// and pepper's PGL. This API is used to manage OpenGL contexts in the Chrome
// renderer process in a way that is consistent with other platforms. It is
// a C style API to ease porting of existing OpenGL software to Chrome.

#ifndef CHROME_RENDERER_GGL_GGL_H_
#define CHROME_RENDERER_GGL_GGL_H_
#pragma once

#include "base/callback.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"

class GpuChannelHost;
class MessageLoop;

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}
}

namespace media {
class VideoDecodeContext;
class VideoDecodeEngine;
}

namespace ggl {

class Context;

// These are the same error codes as used by EGL.
enum Error {
  SUCCESS             = 0x3000,
  NOT_INITIALIZED     = 0x3001,
  BAD_ATTRIBUTE       = 0x3004,
  BAD_CONTEXT         = 0x3006,
  CONTEXT_LOST        = 0x300E
};

// Context configuration attributes. These are the same as used by EGL.
// Attributes are matched using a closest fit algorithm.
const int32 GGL_ALPHA_SIZE     = 0x3021;
const int32 GGL_BLUE_SIZE      = 0x3022;
const int32 GGL_GREEN_SIZE     = 0x3023;
const int32 GGL_RED_SIZE       = 0x3024;
const int32 GGL_DEPTH_SIZE     = 0x3025;
const int32 GGL_STENCIL_SIZE   = 0x3026;
const int32 GGL_SAMPLES        = 0x3031;
const int32 GGL_SAMPLE_BUFFERS = 0x3032;
const int32 GGL_NONE           = 0x3038;  // Attrib list = terminator

// Initialize the GGL library. This must have completed before any other GGL
// functions are invoked.
bool Initialize();

// Terminate the GGL library. This must be called after any other GGL functions
// have completed.
bool Terminate();

// Create a GGL context that renders directly to a view. The view and the
// associated window must not be destroyed until the returned context has been
// destroyed, otherwise the GPU process might attempt to render to an invalid
// window handle.
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
Context* CreateViewContext(GpuChannelHost* channel,
                           int render_view_id,
                           const char* allowed_extensions,
                           const int32* attrib_list);

#if defined(OS_MACOSX)
// On Mac OS X only, view contexts actually behave like offscreen contexts, and
// require an explicit resize operation which is slightly different from that
// of offscreen contexts.
void ResizeOnscreenContext(Context* context, const gfx::Size& size);
#endif

// Create a GGL context that renders to an offscreen frame buffer. If parent is
// not NULL, that context can access a copy of the created
// context's frame buffer that is updated every time SwapBuffers is called. It
// is not as general as shared contexts in other implementations of OpenGL. If
// parent is not NULL, it must be used on the same thread as the parent. A child
// context may not outlive its parent.  attrib_list must be NULL or a
// GGL_NONE-terminated list of attribute/value pairs.
Context* CreateOffscreenContext(GpuChannelHost* channel,
                                Context* parent,
                                const gfx::Size& size,
                                const char* allowed_extensions,
                                const int32* attrib_list);

// Resize an offscreen frame buffer. The resize occurs on the next call to
// SwapBuffers. This is to avoid waiting until all pending GL calls have been
// executed by the GPU process. Everything rendered up to the call to
// SwapBuffers will be lost. A lost context will be reported if the resize
// fails.
void ResizeOffscreenContext(Context* context, const gfx::Size& size);

// For an offscreen frame buffer context, return the texture ID with
// respect to the parent context. Returns zero if context does not have a
// parent.
uint32 GetParentTextureId(Context* context);

// Create a new texture in the parent's context.  Returns zero if context
// does not have a parent.
uint32 CreateParentTexture(Context* context, const gfx::Size& size);

// Deletes a texture in the parent's context.
void DeleteParentTexture(Context* context, uint32 texture);

// Provides a callback that will be invoked when SwapBuffers has completed
// service side.
void SetSwapBuffersCallback(Context* context, Callback0::Type* callback);

// Set the current GGL context for the calling thread.
bool MakeCurrent(Context* context);

// For a view context, display everything that has been rendered since the
// last call. For an offscreen context, resolve everything that has been
// rendered since the last call to a copy that can be accessed by the parent
// context.
bool SwapBuffers(Context* context);

// Destroy the given GGL context.
bool DestroyContext(Context* context);

// Create a hardware video decode engine corresponding to the context.
media::VideoDecodeEngine* CreateVideoDecodeEngine(Context* context);

// Create a hardware video decode context to pair with the hardware video
// decode engine. It can also be used with a software decode engine.
//
// Set |hardware_decoder| to true if this context is for a hardware video
// engine. |message_loop| is where the decode context should run on.
media::VideoDecodeContext* CreateVideoDecodeContext(Context* context,
                                                    MessageLoop* message_loop,
                                                    bool hardware_decoder);

// TODO(gman): Remove this
void DisableShaderTranslation(Context* context);

// Allows direct access to the GLES2 implementation so a context
// can be used without making it current.
gpu::gles2::GLES2Implementation* GetImplementation(Context* context);

// Return the current GGL error.
Error GetError(Context* context);

// Return true if GPU process reported context lost or there was a problem
// communicating with the GPU process.
bool IsCommandBufferContextLost(Context* context);

}  // namespace ggl

#endif  // CHROME_RENDERER_GGL_GGL_H_
