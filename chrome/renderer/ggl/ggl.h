// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This API is consistent with other OpenGL setup APIs like window's WGL
// and pepper's PGL. This API is used to manage OpenGL contexts in the Chrome
// renderer process in a way that is consistent with other platforms. It is
// a C style API to ease porting of existing OpenGL software to Chrome.

#ifndef CHROME_RENDERER_GGL_GGL_H_
#define CHROME_RENDERER_GGL_GGL_H_

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

// Create A GGL context for an offscreen 1 x 1 pbuffer.
Context* CreateContext(GpuChannelHost* channel);

// Set the current GGL context for the calling thread.
bool MakeCurrent(Context* context);

// Get the calling thread's current GGL context.
Context* GetCurrentContext();

// Display everything that has been rendered since the last call.
bool SwapBuffers();

// Destroy the given GGL context.
bool DestroyContext(Context* context);

// Return the current GGL error.
Error GetError();

}  // namespace ggl

#endif  // CHROME_RENDERER_GGL_GGL_H_
