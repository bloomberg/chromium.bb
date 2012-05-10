// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// GL_CHROMIUM_flipy
#define GL_UNPACK_FLIP_Y_CHROMIUM             0x9240

#define GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM     0x9241
#define GL_UNPACK_COLORSPACE_CONVERSION_CHROMIUM 0x9243

// GL_ANGLE_pack_reverse_row_order
#define GL_PACK_REVERSE_ROW_ORDER_ANGLE    0x93A4

// GL_ANGLE_texture_usage
#define GL_TEXTURE_USAGE_ANGLE                 0x93A2
#define GL_FRAMEBUFFER_ATTACHMENT_ANGLE        0x93A3

// GL_EXT_texture_storage
#define GL_TEXTURE_IMMUTABLE_FORMAT_EXT        0x912F
#define GL_ALPHA8_EXT                          0x803C
#define GL_LUMINANCE8_EXT                      0x8040
#define GL_LUMINANCE8_ALPHA8_EXT               0x8045
#define GL_RGBA32F_EXT                         0x8814
#define GL_RGB32F_EXT                          0x8815
#define GL_ALPHA32F_EXT                        0x8816
#define GL_LUMINANCE32F_EXT                    0x8818
#define GL_LUMINANCE_ALPHA32F_EXT              0x8819
#define GL_RGBA16F_EXT                         0x881A
#define GL_RGB16F_EXT                          0x881B
#define GL_ALPHA16F_EXT                        0x881C
#define GL_LUMINANCE16F_EXT                    0x881E
#define GL_LUMINANCE_ALPHA16F_EXT              0x881F
#define GL_BGRA8_EXT                           0x93A1

// GL_ANGLE_instanced_arrays
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE   0x88FE

// GL_EXT_occlusion_query_boolean
#define GL_ANY_SAMPLES_PASSED_EXT              0x8C2F
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT 0x8D6A
#define GL_CURRENT_QUERY_EXT                   0x8865
#define GL_QUERY_RESULT_EXT                    0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT          0x8867

// GL_CHROMIUM_command_buffer_query
#define GL_COMMANDS_ISSUED_CHROMIUM            0x84F2

// GL_OES_texure_3D
#define GL_SAMPLER_3D_OES                                       0x8B5F

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
