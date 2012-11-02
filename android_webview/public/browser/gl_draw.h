// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_PUBLIC_BROWSER_GL_DRAW_H_
#define ANDROID_WEBVIEW_PUBLIC_BROWSER_GL_DRAW_H_

#ifdef __cplusplus
extern "C" {
#endif

// Holds the information required to trigger an OpenGL drawing operation.
struct AwGLDrawInfo {

  // Input: current clip rect.
  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;

  // Input: current width/height of destination surface.
  int width;
  int height;

  // Input: is the render target an FBO.
  bool is_layer;

  // Input: current transform matrix, in OpenGL format.
  float transform[16];

  // Output: dirty region to redraw.
  float dirty_left;
  float dirty_top;
  float dirty_right;
  float dirty_bottom;
};

// Function to invoke a direct GL draw into the client's pre-configured
// GL context. Obtained via AwContents.getGLDrawFunction() (static).
// |view_context| is an opaque pointer that was returned by the corresponding
// call to AwContents.onPrepareGlDraw().
// |draw_info| carries the in and out parameters for this draw.
// |spare| ignored; pass NULL.
typedef void (*AwGLDrawFunction)(void* view_context,
                                 const GLDrawInfo* draw_info,
                                 void* spare);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ANDROID_WEBVIEW_PUBLIC_BROWSER_GL_DRAW_H_
