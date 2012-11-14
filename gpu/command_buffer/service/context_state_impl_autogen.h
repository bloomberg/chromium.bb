// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

// It is included by context_state.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_

ContextState::EnableFlags::EnableFlags()
    : blend(false),
      cull_face(false),
      depth_test(false),
      dither(true),
      polygon_offset_fill(false),
      sample_alpha_to_coverage(false),
      sample_coverage(false),
      scissor_test(false),
      stencil_test(false) {
}

void ContextState::Initialize() {
  blend_color_red = 0.0f;
  blend_color_green = 0.0f;
  blend_color_blue = 0.0f;
  blend_color_alpha = 0.0f;
  blend_equation_rgb = GL_FUNC_ADD;
  blend_equation_alpha = GL_FUNC_ADD;
  blend_source_rgb = GL_ONE;
  blend_dest_rgb = GL_ZERO;
  blend_source_alpha = GL_ONE;
  blend_dest_alpha = GL_ZERO;
  color_clear_red = 0.0f;
  color_clear_green = 0.0f;
  color_clear_blue = 0.0f;
  color_clear_alpha = 0.0f;
  depth_clear = 1.0f;
  stencil_clear = 0;
  color_mask_red = true;
  color_mask_green = true;
  color_mask_blue = true;
  color_mask_alpha = true;
  cull_mode = GL_BACK;
  depth_func = GL_LESS;
  depth_mask = true;
  z_near = 0.0f;
  z_far = 1.0f;
  front_face = GL_CCW;
  line_width = 1.0f;
  polygon_offset_factor = 0.0f;
  polygon_offset_units = 0.0f;
  sample_coverage_value = 1.0f;
  sample_coverage_invert = false;
  scissor_x = 0;
  scissor_y = 0;
  scissor_width = 1;
  scissor_height = 1;
  stencil_front_func = GL_ALWAYS;
  stencil_front_ref = 0;
  stencil_front_mask = 0xFFFFFFFFU;
  stencil_back_func = GL_ALWAYS;
  stencil_back_ref = 0;
  stencil_back_mask = 0xFFFFFFFFU;
  stencil_front_writemask = 0xFFFFFFFFU;
  stencil_back_writemask = 0xFFFFFFFFU;
  stencil_front_fail_op = GL_KEEP;
  stencil_front_z_fail_op = GL_KEEP;
  stencil_front_z_pass_op = GL_KEEP;
  stencil_back_fail_op = GL_KEEP;
  stencil_back_z_fail_op = GL_KEEP;
  stencil_back_z_pass_op = GL_KEEP;
  viewport_x = 0;
  viewport_y = 0;
  viewport_width = 1;
  viewport_height = 1;
}

void ContextState::InitCapabilities() const {
  EnableDisable(GL_BLEND, enable_flags.blend);
  EnableDisable(GL_CULL_FACE, enable_flags.cull_face);
  EnableDisable(GL_DEPTH_TEST, enable_flags.depth_test);
  EnableDisable(GL_DITHER, enable_flags.dither);
  EnableDisable(GL_POLYGON_OFFSET_FILL, enable_flags.polygon_offset_fill);
  EnableDisable(
      GL_SAMPLE_ALPHA_TO_COVERAGE, enable_flags.sample_alpha_to_coverage);
  EnableDisable(GL_SAMPLE_COVERAGE, enable_flags.sample_coverage);
  EnableDisable(GL_SCISSOR_TEST, enable_flags.scissor_test);
  EnableDisable(GL_STENCIL_TEST, enable_flags.stencil_test);
}

void ContextState::InitState() const {
  glBlendColor(
      blend_color_red, blend_color_green, blend_color_blue, blend_color_alpha);
  glBlendEquationSeparate(blend_equation_rgb, blend_equation_alpha);
  glBlendFuncSeparate(
      blend_source_rgb, blend_dest_rgb, blend_source_alpha, blend_dest_alpha);
  glClearColor(
      color_clear_red, color_clear_green, color_clear_blue, color_clear_alpha);
  glClearDepth(depth_clear);
  glClearStencil(stencil_clear);
  glColorMask(
      color_mask_red, color_mask_green, color_mask_blue, color_mask_alpha);
  glCullFace(cull_mode);
  glDepthFunc(depth_func);
  glDepthMask(depth_mask);
  glDepthRange(z_near, z_far);
  glFrontFace(front_face);
  glLineWidth(line_width);
  glPolygonOffset(polygon_offset_factor, polygon_offset_units);
  glSampleCoverage(sample_coverage_value, sample_coverage_invert);
  glScissor(scissor_x, scissor_y, scissor_width, scissor_height);
  glStencilFuncSeparate(
      GL_FRONT, stencil_front_func, stencil_front_ref, stencil_front_mask);
  glStencilFuncSeparate(
      GL_BACK, stencil_back_func, stencil_back_ref, stencil_back_mask);
  glStencilMaskSeparate(GL_FRONT, stencil_front_writemask);
  glStencilMaskSeparate(GL_BACK, stencil_back_writemask);
  glStencilOpSeparate(
      GL_FRONT, stencil_front_fail_op, stencil_front_z_fail_op,
      stencil_front_z_pass_op);
  glStencilOpSeparate(
      GL_BACK, stencil_back_fail_op, stencil_back_z_fail_op,
      stencil_back_z_pass_op);
  glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_

