// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// It is included by context_state.h
#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_AUTOGEN_H_

struct EnableFlags {
  EnableFlags();
  bool blend;
  bool cull_face;
  bool depth_test;
  bool dither;
  bool polygon_offset_fill;
  bool sample_alpha_to_coverage;
  bool sample_coverage;
  bool scissor_test;
  bool stencil_test;
};

GLfloat blend_color_red;
GLfloat blend_color_green;
GLfloat blend_color_blue;
GLfloat blend_color_alpha;
GLenum blend_equation_rgb;
GLenum blend_equation_alpha;
GLenum blend_source_rgb;
GLenum blend_dest_rgb;
GLenum blend_source_alpha;
GLenum blend_dest_alpha;
GLfloat color_clear_red;
GLfloat color_clear_green;
GLfloat color_clear_blue;
GLfloat color_clear_alpha;
GLclampf depth_clear;
GLint stencil_clear;
GLboolean color_mask_red;
GLboolean color_mask_green;
GLboolean color_mask_blue;
GLboolean color_mask_alpha;
GLenum cull_mode;
GLenum depth_func;
GLboolean depth_mask;
GLclampf z_near;
GLclampf z_far;
GLenum front_face;
GLfloat line_width;
GLfloat polygon_offset_factor;
GLfloat polygon_offset_units;
GLclampf sample_coverage_value;
GLboolean sample_coverage_invert;
GLint scissor_x;
GLint scissor_y;
GLsizei scissor_width;
GLsizei scissor_height;
GLenum stencil_front_func;
GLint stencil_front_ref;
GLuint stencil_front_mask;
GLenum stencil_back_func;
GLint stencil_back_ref;
GLuint stencil_back_mask;
GLuint stencil_front_writemask;
GLuint stencil_back_writemask;
GLenum stencil_front_fail_op;
GLenum stencil_front_z_fail_op;
GLenum stencil_front_z_pass_op;
GLenum stencil_back_fail_op;
GLenum stencil_back_z_fail_op;
GLenum stencil_back_z_pass_op;
GLint viewport_x;
GLint viewport_y;
GLsizei viewport_width;
GLsizei viewport_height;

#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_AUTOGEN_H_

