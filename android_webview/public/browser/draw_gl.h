// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_GL_H_
#define ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_GL_H_

#ifdef __cplusplus
extern "C" {
#endif

// Holds the information required to trigger an OpenGL drawing operation.
struct AwDrawGLInfo {
  // Input: tells the draw function what action to perform.
  enum Mode {
    kModeDraw,
    kModeProcess,
  } mode;

  // Input: current clip rect in surface coordinates. Reflects the current state
  // of the OpenGL scissor rect. Both the OpenGL scissor rect and viewport are
  // set by the caller of the draw function and updated during View animations.
  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;

  // Input: current width/height of destination surface.
  int width;
  int height;

  // Input: is the View rendered into an independent layer.
  // If false, the surface is likely to hold to the full screen contents, with
  // the scissor box set by the caller to the actual View location and size.
  // Also the transformation matrix will contain at least a translation to the
  // position of the View to render, plus any other transformations required as
  // part of any ongoing View animation. View translucency (alpha) is ignored,
  // although the framework will set is_layer to true for non-opaque cases.
  // Can be requested via the View.setLayerType(View.LAYER_TYPE_NONE, ...)
  // Android API method.
  //
  // If true, the surface is dedicated to the View and should have its size.
  // The viewport and scissor box are set by the caller to the whole surface.
  // Animation transformations are handled by the caller and not reflected in
  // the provided transformation matrix. Translucency works normally.
  // Can be requested via the View.setLayerType(View.LAYER_TYPE_HARDWARE, ...)
  // Android API method.
  bool is_layer;

  // Input: current transformation matrix in surface pixels.
  // Uses the column-based OpenGL matrix format.
  float transform[16];

  // Output: tells the caller what to do next.
  enum StatusMask {
    kStatusMaskDone = 0x0,
    kStatusMaskDraw = 0x1,
    kStatusMaskInvoke = 0x2,
  };

  // Output: mask indicating the status after calling the functor.
  unsigned int status_mask;

  // Output: dirty region to redraw in surface coordinates.
  float dirty_left;
  float dirty_top;
  float dirty_right;
  float dirty_bottom;
};

// Function to invoke a direct GL draw into the client's pre-configured
// GL context. Obtained via AwContents.getDrawGLFunction() (static).
// |view_context| is an opaque identifier that was returned by the corresponding
// call to AwContents.getAwDrawGLViewContext().
// |draw_info| carries the in and out parameters for this draw.
// |spare| ignored; pass NULL.
typedef void (AwDrawGLFunction)(int view_context,
                                AwDrawGLInfo* draw_info,
                                void* spare);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_GL_H_
