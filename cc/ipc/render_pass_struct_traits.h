// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_RENDER_PASS_STRUCT_TRAITS_H_
#define CC_IPC_RENDER_PASS_STRUCT_TRAITS_H_

#include "base/logging.h"
#include "cc/ipc/quads_struct_traits.h"
#include "cc/ipc/render_pass.mojom-shared.h"
#include "cc/quads/render_pass.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/mojo/transform_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::RenderPassDataView,
                    std::unique_ptr<cc::RenderPass>> {
  static int32_t id(const std::unique_ptr<cc::RenderPass>& input) {
    DCHECK(input->id);
    return input->id;
  }

  static const gfx::Rect& output_rect(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->output_rect;
  }

  static const gfx::Rect& damage_rect(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->damage_rect;
  }

  static const gfx::Transform& transform_to_root_target(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->transform_to_root_target;
  }

  static const cc::FilterOperations& filters(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->filters;
  }

  static const cc::FilterOperations& background_filters(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->background_filters;
  }

  static const gfx::ColorSpace& color_space(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->color_space;
  }

  static bool has_transparent_background(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->has_transparent_background;
  }

  static const cc::QuadList& quad_list(
      const std::unique_ptr<cc::RenderPass>& input) {
    return input->quad_list;
  }

  static bool Read(cc::mojom::RenderPassDataView data,
                   std::unique_ptr<cc::RenderPass>* out);
};

}  // namespace mojo

#endif  // CC_IPC_RENDER_PASS_STRUCT_TRAITS_H_
