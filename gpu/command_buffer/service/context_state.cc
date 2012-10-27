// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

namespace gpu {
namespace gles2 {

TextureUnit::TextureUnit()
    : bind_target(GL_TEXTURE_2D) {
}

TextureUnit::~TextureUnit() {
}

ContextState::ContextState()
    : pack_alignment(4),
      unpack_alignment(4),
      active_texture_unit(0),
      color_clear_red(0),
      color_clear_green(0),
      color_clear_blue(0),
      color_clear_alpha(0),
      color_mask_red(true),
      color_mask_green(true),
      color_mask_blue(true),
      color_mask_alpha(true),
      depth_clear(1.0f),
      depth_mask(true),
      depth_func(GL_LESS),
      z_near(0.0f),
      z_far(1.0f),
      enable_blend(false),
      enable_cull_face(false),
      enable_scissor_test(false),
      enable_depth_test(false),
      enable_stencil_test(false),
      enable_polygon_offset_fill(false),
      enable_dither(true),
      enable_sample_alpha_to_coverage(false),
      enable_sample_coverage(false),
      viewport_x(0),
      viewport_y(0),
      viewport_width(0),
      viewport_height(0),
      viewport_max_width(0),
      viewport_max_height(0),
      scissor_x(0),
      scissor_y(0),
      scissor_width(0),
      scissor_height(0),
      cull_mode(GL_BACK),
      front_face(GL_CCW),
      blend_source_rgb(GL_ONE),
      blend_dest_rgb(GL_ZERO),
      blend_source_alpha(GL_ONE),
      blend_dest_alpha(GL_ZERO),
      blend_equation_rgb(GL_FUNC_ADD),
      blend_equation_alpha(GL_FUNC_ADD),
      blend_color_red(0),
      blend_color_green(0),
      blend_color_blue(0),
      blend_color_alpha(0),
      stencil_clear(0),
      stencil_front_writemask(0xFFFFFFFFU),
      stencil_front_func(GL_ALWAYS),
      stencil_front_ref(0),
      stencil_front_mask(0xFFFFFFFFU),
      stencil_front_fail_op(GL_KEEP),
      stencil_front_z_fail_op(GL_KEEP),
      stencil_front_z_pass_op(GL_KEEP),
      stencil_back_writemask(0xFFFFFFFFU),
      stencil_back_func(GL_ALWAYS),
      stencil_back_ref(0),
      stencil_back_mask(0xFFFFFFFFU),
      stencil_back_fail_op(GL_KEEP),
      stencil_back_z_fail_op(GL_KEEP),
      stencil_back_z_pass_op(GL_KEEP),
      polygon_offset_factor(0.0f),
      polygon_offset_units(0.0f),
      sample_coverage_value(1.0f),
      sample_coverage_invert(false),
      line_width(1.0),
      hint_generate_mipmap(GL_DONT_CARE),
      hint_fragment_shader_derivative(GL_DONT_CARE),
      pack_reverse_row_order(false) {
}

ContextState::~ContextState() {
}

}  // namespace gles2
}  // namespace gpu


