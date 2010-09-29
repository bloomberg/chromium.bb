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
  BAD_CONTEXT         = 0x3006,
  CONTEXT_LOST        = 0x300E
};

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
                           gfx::NativeViewId view,
                           int render_view_id);

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
// context may not outlive its parent.
Context* CreateOffscreenContext(GpuChannelHost* channel,
                                Context* parent,
                                const gfx::Size& size);

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
void SetSwapBuffersCallback(Context* context,
                            Callback1<Context*>::Type* callback);

// Set the current GGL context for the calling thread.
bool MakeCurrent(Context* context);

// Get the calling thread's current GGL context.
Context* GetCurrentContext();

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
// engine.
media::VideoDecodeContext* CreateVideoDecodeContext(Context* context,
                                                    bool hardware_decoder);

// TODO(gman): Remove this
void DisableShaderTranslation(Context* context);

// Return the current GGL error.
Error GetError();

}  // namespace ggl

#endif  // CHROME_RENDERER_GGL_GGL_H_
