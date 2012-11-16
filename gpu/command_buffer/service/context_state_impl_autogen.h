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
bool ContextState::GetEnabled(GLenum cap) const {
  switch (cap) {
    case GL_BLEND:
      return enable_flags.blend;
    case GL_CULL_FACE:
      return enable_flags.cull_face;
    case GL_DEPTH_TEST:
      return enable_flags.depth_test;
    case GL_DITHER:
      return enable_flags.dither;
    case GL_POLYGON_OFFSET_FILL:
      return enable_flags.polygon_offset_fill;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      return enable_flags.sample_alpha_to_coverage;
    case GL_SAMPLE_COVERAGE:
      return enable_flags.sample_coverage;
    case GL_SCISSOR_TEST:
      return enable_flags.scissor_test;
    case GL_STENCIL_TEST:
      return enable_flags.stencil_test;
    default:
      GPU_NOTREACHED();
      return false;
  }
}

bool ContextState::GetStateAsGLint(
    GLenum pname, GLint* params, GLsizei* num_written) const {
  switch (pname) {
    case GL_VIEWPORT:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(viewport_x);
        params[1] = static_cast<GLint>(viewport_y);
        params[2] = static_cast<GLint>(viewport_width);
        params[3] = static_cast<GLint>(viewport_height);
      }
      return true;
    case GL_BLEND_SRC_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_source_rgb);
      }
      return true;
    case GL_BLEND_DST_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_dest_rgb);
      }
      return true;
    case GL_BLEND_SRC_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_source_alpha);
      }
      return true;
    case GL_BLEND_DST_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_dest_alpha);
      }
      return true;
    case GL_LINE_WIDTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(line_width);
      }
      return true;
    case GL_BLEND_COLOR:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(blend_color_red);
        params[1] = static_cast<GLint>(blend_color_green);
        params[2] = static_cast<GLint>(blend_color_blue);
        params[3] = static_cast<GLint>(blend_color_alpha);
      }
      return true;
    case GL_STENCIL_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_clear);
      }
      return true;
    case GL_COLOR_WRITEMASK:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(color_mask_red);
        params[1] = static_cast<GLint>(color_mask_green);
        params[2] = static_cast<GLint>(color_mask_blue);
        params[3] = static_cast<GLint>(color_mask_alpha);
      }
      return true;
    case GL_COLOR_CLEAR_VALUE:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(color_clear_red);
        params[1] = static_cast<GLint>(color_clear_green);
        params[2] = static_cast<GLint>(color_clear_blue);
        params[3] = static_cast<GLint>(color_clear_alpha);
      }
      return true;
    case GL_DEPTH_RANGE:
      *num_written = 2;
      if (params) {
        params[0] = static_cast<GLint>(z_near);
        params[1] = static_cast<GLint>(z_far);
      }
      return true;
    case GL_DEPTH_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(depth_clear);
      }
      return true;
    case GL_STENCIL_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_z_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_z_pass_op);
      }
      return true;
    case GL_STENCIL_BACK_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_z_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_z_pass_op);
      }
      return true;
    case GL_SCISSOR_BOX:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLint>(scissor_x);
        params[1] = static_cast<GLint>(scissor_y);
        params[2] = static_cast<GLint>(scissor_width);
        params[3] = static_cast<GLint>(scissor_height);
      }
      return true;
    case GL_FRONT_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(front_face);
      }
      return true;
    case GL_SAMPLE_COVERAGE_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(sample_coverage_value);
      }
      return true;
    case GL_SAMPLE_COVERAGE_INVERT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(sample_coverage_invert);
      }
      return true;
    case GL_POLYGON_OFFSET_FACTOR:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(polygon_offset_factor);
      }
      return true;
    case GL_POLYGON_OFFSET_UNITS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(polygon_offset_units);
      }
      return true;
    case GL_CULL_FACE_MODE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(cull_mode);
      }
      return true;
    case GL_DEPTH_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(depth_func);
      }
      return true;
    case GL_STENCIL_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_func);
      }
      return true;
    case GL_STENCIL_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_ref);
      }
      return true;
    case GL_STENCIL_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_mask);
      }
      return true;
    case GL_STENCIL_BACK_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_func);
      }
      return true;
    case GL_STENCIL_BACK_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_ref);
      }
      return true;
    case GL_STENCIL_BACK_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_mask);
      }
      return true;
    case GL_DEPTH_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(depth_mask);
      }
      return true;
    case GL_BLEND_EQUATION_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_equation_rgb);
      }
      return true;
    case GL_BLEND_EQUATION_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(blend_equation_alpha);
      }
      return true;
    case GL_STENCIL_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_front_writemask);
      }
      return true;
    case GL_STENCIL_BACK_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(stencil_back_writemask);
      }
      return true;
    case GL_BLEND:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.blend);
      }
      return true;
    case GL_CULL_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.cull_face);
      }
      return true;
    case GL_DEPTH_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.depth_test);
      }
      return true;
    case GL_DITHER:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.dither);
      }
      return true;
    case GL_POLYGON_OFFSET_FILL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.polygon_offset_fill);
      }
      return true;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.sample_alpha_to_coverage);
      }
      return true;
    case GL_SAMPLE_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.sample_coverage);
      }
      return true;
    case GL_SCISSOR_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.scissor_test);
      }
      return true;
    case GL_STENCIL_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLint>(enable_flags.stencil_test);
      }
      return true;
    default:
      return false;
  }
}

bool ContextState::GetStateAsGLfloat(
    GLenum pname, GLfloat* params, GLsizei* num_written) const {
  switch (pname) {
    case GL_VIEWPORT:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(viewport_x);
        params[1] = static_cast<GLfloat>(viewport_y);
        params[2] = static_cast<GLfloat>(viewport_width);
        params[3] = static_cast<GLfloat>(viewport_height);
      }
      return true;
    case GL_BLEND_SRC_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_source_rgb);
      }
      return true;
    case GL_BLEND_DST_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_dest_rgb);
      }
      return true;
    case GL_BLEND_SRC_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_source_alpha);
      }
      return true;
    case GL_BLEND_DST_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_dest_alpha);
      }
      return true;
    case GL_LINE_WIDTH:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(line_width);
      }
      return true;
    case GL_BLEND_COLOR:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_color_red);
        params[1] = static_cast<GLfloat>(blend_color_green);
        params[2] = static_cast<GLfloat>(blend_color_blue);
        params[3] = static_cast<GLfloat>(blend_color_alpha);
      }
      return true;
    case GL_STENCIL_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_clear);
      }
      return true;
    case GL_COLOR_WRITEMASK:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(color_mask_red);
        params[1] = static_cast<GLfloat>(color_mask_green);
        params[2] = static_cast<GLfloat>(color_mask_blue);
        params[3] = static_cast<GLfloat>(color_mask_alpha);
      }
      return true;
    case GL_COLOR_CLEAR_VALUE:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(color_clear_red);
        params[1] = static_cast<GLfloat>(color_clear_green);
        params[2] = static_cast<GLfloat>(color_clear_blue);
        params[3] = static_cast<GLfloat>(color_clear_alpha);
      }
      return true;
    case GL_DEPTH_RANGE:
      *num_written = 2;
      if (params) {
        params[0] = static_cast<GLfloat>(z_near);
        params[1] = static_cast<GLfloat>(z_far);
      }
      return true;
    case GL_DEPTH_CLEAR_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(depth_clear);
      }
      return true;
    case GL_STENCIL_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_z_fail_op);
      }
      return true;
    case GL_STENCIL_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_z_pass_op);
      }
      return true;
    case GL_STENCIL_BACK_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_z_fail_op);
      }
      return true;
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_z_pass_op);
      }
      return true;
    case GL_SCISSOR_BOX:
      *num_written = 4;
      if (params) {
        params[0] = static_cast<GLfloat>(scissor_x);
        params[1] = static_cast<GLfloat>(scissor_y);
        params[2] = static_cast<GLfloat>(scissor_width);
        params[3] = static_cast<GLfloat>(scissor_height);
      }
      return true;
    case GL_FRONT_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(front_face);
      }
      return true;
    case GL_SAMPLE_COVERAGE_VALUE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(sample_coverage_value);
      }
      return true;
    case GL_SAMPLE_COVERAGE_INVERT:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(sample_coverage_invert);
      }
      return true;
    case GL_POLYGON_OFFSET_FACTOR:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(polygon_offset_factor);
      }
      return true;
    case GL_POLYGON_OFFSET_UNITS:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(polygon_offset_units);
      }
      return true;
    case GL_CULL_FACE_MODE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(cull_mode);
      }
      return true;
    case GL_DEPTH_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(depth_func);
      }
      return true;
    case GL_STENCIL_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_func);
      }
      return true;
    case GL_STENCIL_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_ref);
      }
      return true;
    case GL_STENCIL_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_mask);
      }
      return true;
    case GL_STENCIL_BACK_FUNC:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_func);
      }
      return true;
    case GL_STENCIL_BACK_REF:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_ref);
      }
      return true;
    case GL_STENCIL_BACK_VALUE_MASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_mask);
      }
      return true;
    case GL_DEPTH_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(depth_mask);
      }
      return true;
    case GL_BLEND_EQUATION_RGB:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_equation_rgb);
      }
      return true;
    case GL_BLEND_EQUATION_ALPHA:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(blend_equation_alpha);
      }
      return true;
    case GL_STENCIL_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_front_writemask);
      }
      return true;
    case GL_STENCIL_BACK_WRITEMASK:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(stencil_back_writemask);
      }
      return true;
    case GL_BLEND:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.blend);
      }
      return true;
    case GL_CULL_FACE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.cull_face);
      }
      return true;
    case GL_DEPTH_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.depth_test);
      }
      return true;
    case GL_DITHER:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.dither);
      }
      return true;
    case GL_POLYGON_OFFSET_FILL:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.polygon_offset_fill);
      }
      return true;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] =
            static_cast<GLfloat>(enable_flags.sample_alpha_to_coverage);
      }
      return true;
    case GL_SAMPLE_COVERAGE:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.sample_coverage);
      }
      return true;
    case GL_SCISSOR_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.scissor_test);
      }
      return true;
    case GL_STENCIL_TEST:
      *num_written = 1;
      if (params) {
        params[0] = static_cast<GLfloat>(enable_flags.stencil_test);
      }
      return true;
    default:
      return false;
  }
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_IMPL_AUTOGEN_H_

