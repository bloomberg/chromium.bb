// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some GLES2 constants. There is a Chromium-specific need
// to have them in a separate file.

#ifndef GPU_GLES2_GL2CHROMIUM_CONSTANTS_H_
#define GPU_GLES2_GL2CHROMIUM_CONSTANTS_H_

#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

/* GL_EXT_discard_framebuffer */
#ifndef GL_EXT_discard_framebuffer
#define GL_COLOR_EXT                                            0x1800
#define GL_DEPTH_EXT                                            0x1801
#define GL_STENCIL_EXT                                          0x1802
#endif

#endif  // GPU_GLES2_GL2CHROMIUM_CONSTANTS_H_

