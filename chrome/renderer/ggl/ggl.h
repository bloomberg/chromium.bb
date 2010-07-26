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

#include "gfx/native_widget_types.h"
#include "gfx/size.h"

class GpuChannelHost;

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

// Create a GGL context that renders directly to a view.
Context* CreateViewContext(GpuChannelHost* channel, gfx::NativeViewId view);

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

// TODO(gman): Remove this
void DisableShaderTranslation(Context* context);

// Return the current GGL error.
Error GetError();

}  // namespace ggl

#endif  // CHROME_RENDERER_GGL_GGL_H_
