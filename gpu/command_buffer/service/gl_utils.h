// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file includes all the necessary GL headers and implements some useful
// utilities.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_

#include "build/build_config.h"
#include "ui/gfx/gl/gl_bindings.h"

// GLES2 defines not part of Desktop GL
// Shader Precision-Specified Types
#define GL_LOW_FLOAT                      0x8DF0
#define GL_MEDIUM_FLOAT                   0x8DF1
#define GL_HIGH_FLOAT                     0x8DF2
#define GL_LOW_INT                        0x8DF3
#define GL_MEDIUM_INT                     0x8DF4
#define GL_HIGH_INT                       0x8DF5
#define GL_IMPLEMENTATION_COLOR_READ_TYPE   0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT 0x8B9B
#define GL_MAX_FRAGMENT_UNIFORM_VECTORS     0x8DFD
#define GL_MAX_VERTEX_UNIFORM_VECTORS       0x8DFB
#define GL_MAX_VARYING_VECTORS              0x8DFC
#define GL_SHADER_BINARY_FORMATS          0x8DF8
#define GL_NUM_SHADER_BINARY_FORMATS      0x8DF9
#define GL_SHADER_COMPILER                0x8DFA
#define GL_RGB565                         0x8D62
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES 0x8B8B
#define GL_RGB8_OES                            0x8051
#define GL_RGBA8_OES                           0x8058
#define GL_HALF_FLOAT_OES                      0x8D61

// GL_OES_EGL_image_external
#define GL_TEXTURE_EXTERNAL_OES                0x8D65
#define GL_SAMPLER_EXTERNAL_OES                0x8D66
#define GL_TEXTURE_BINDING_EXTERNAL_OES        0x8D67
#define GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES    0x8D68

// GL_ANGLE_translated_shader_source
#define GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE 0x93A0

// GL_ANGLE_pack_reverse_row_order
#define GL_PACK_REVERSE_ROW_ORDER_ANGLE    0x93A4

#define GL_GLEXT_PROTOTYPES 1

// Define this for extra GL error debugging (slower).
// #define GL_ERROR_DEBUGGING
#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#endif  // GL_ERROR_DEBUGGING

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_
