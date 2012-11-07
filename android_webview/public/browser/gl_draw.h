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
  // Input: Tells the Draw function what action to perform.
  enum Mode {
    kModeDraw,
    kModeProcess,
  } mode;

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

  // Output: tells the caller what to do next.
  enum StatusMask {
    kStatusMaskDone = 0x0,
    kStatusMaskDraw = 0x1,
    kStatusMaskInvoke = 0x2,
    kStatusMaskDrew = 0x4,
  } status_mask;

  // Output: dirty region to redraw.
  float dirty_left;
  float dirty_top;
  float dirty_right;
  float dirty_bottom;
};

// Function to invoke a direct GL draw into the client's pre-configured
// GL context. Obtained via AwContents.getGLDrawFunction() (static).
// |view_context| is an opaque identifier that was returned by the corresponding
// call to AwContents.getAwGLDrawViewContext().
// |draw_info| carries the in and out parameters for this draw.
// |spare| ignored; pass NULL.
typedef void (AwGLDrawFunction)(int view_context,
                                AwGLDrawInfo* draw_info,
                                void* spare);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ANDROID_WEBVIEW_PUBLIC_BROWSER_GL_DRAW_H_
