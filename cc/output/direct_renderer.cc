// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/direct_renderer.h"

#include <vector>

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/base/math_util.h"
#include "cc/quads/draw_quad.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

static gfx::Transform OrthoProjectionMatrix(float left,
                                            float right,
                                            float bottom,
                                            float top) {
  // Use the standard formula to map the clipping frustum to the cube from
  // [-1, -1, -1] to [1, 1, 1].
  float delta_x = right - left;
  float delta_y = top - bottom;
  gfx::Transform proj;
  if (!delta_x || !delta_y)
    return proj;
  proj.matrix().setDouble(0, 0, 2.0f / delta_x);
  proj.matrix().setDouble(0, 3, -(right + left) / delta_x);
  proj.matrix().setDouble(1, 1, 2.0f / delta_y);
  proj.matrix().setDouble(1, 3, -(top + bottom) / delta_y);

  // Z component of vertices is always set to zero as we don't use the depth
  // buffer while drawing.
  proj.matrix().setDouble(2, 2, 0);

  return proj;
}

static gfx::Transform window_matrix(int x, int y, int width, int height) {
  gfx::Transform canvas;

  // Map to window position and scale up to pixel coordinates.
  canvas.Translate3d(x, y, 0);
  canvas.Scale3d(width, height, 0);

  // Map from ([-1, -1] to [1, 1]) -> ([0, 0] to [1, 1])
  canvas.Translate3d(0.5, 0.5, 0.5);
  canvas.Scale3d(0.5, 0.5, 0.5);

  return canvas;
}

namespace cc {

DirectRenderer::DrawingFrame::DrawingFrame()
    : root_render_pass(NULL),
      current_render_pass(NULL),
      current_texture(NULL),
      flipped_y(false) {}

DirectRenderer::DrawingFrame::~DrawingFrame() {}

//
// static
gfx::RectF DirectRenderer::QuadVertexRect() {
  return gfx::RectF(-0.5f, -0.5f, 1.f, 1.f);
}

// static
void DirectRenderer::QuadRectTransform(gfx::Transform* quad_rect_transform,
                                       const gfx::Transform& quad_transform,
                                       const gfx::RectF& quad_rect) {
  *quad_rect_transform = quad_transform;
  quad_rect_transform->Translate(0.5 * quad_rect.width() + quad_rect.x(),
                                 0.5 * quad_rect.height() + quad_rect.y());
  quad_rect_transform->Scale(quad_rect.width(), quad_rect.height());
}

// static
void DirectRenderer::InitializeMatrices(DrawingFrame& frame,
                                        gfx::Rect draw_rect,
                                        bool flip_y) {
  if (flip_y) {
    frame.projection_matrix = OrthoProjectionMatrix(draw_rect.x(),
                                                    draw_rect.right(),
                                                    draw_rect.bottom(),
                                                    draw_rect.y());
  } else {
    frame.projection_matrix = OrthoProjectionMatrix(draw_rect.x(),
                                                    draw_rect.right(),
                                                    draw_rect.y(),
                                                    draw_rect.bottom());
  }
  frame.window_matrix =
      window_matrix(0, 0, draw_rect.width(), draw_rect.height());
  frame.flipped_y = flip_y;
}

// static
gfx::Rect DirectRenderer::MoveScissorToWindowSpace(
    const DrawingFrame& frame, const gfx::RectF& scissor_rect) {
  gfx::Rect scissor_rect_in_canvas_space = gfx::ToEnclosingRect(scissor_rect);
  // The scissor coordinates must be supplied in viewport space so we need to
  // offset by the relative position of the top left corner of the current
  // render pass.
  gfx::Rect framebuffer_output_rect = frame.current_render_pass->output_rect;
  scissor_rect_in_canvas_space.set_x(
      scissor_rect_in_canvas_space.x() - framebuffer_output_rect.x());
  if (frame.flipped_y && !frame.current_texture) {
    scissor_rect_in_canvas_space.set_y(
        framebuffer_output_rect.height() -
        (scissor_rect_in_canvas_space.bottom() - framebuffer_output_rect.y()));
  } else {
    scissor_rect_in_canvas_space.set_y(
        scissor_rect_in_canvas_space.y() - framebuffer_output_rect.y());
  }
  return scissor_rect_in_canvas_space;
}

DirectRenderer::DirectRenderer(RendererClient* client,
                               ResourceProvider* resource_provider)
    : Renderer(client),
      resource_provider_(resource_provider) {}

DirectRenderer::~DirectRenderer() {}

void DirectRenderer::SetEnlargePassTextureAmountForTesting(
    gfx::Vector2d amount) {
  enlarge_pass_texture_amount_ = amount;
}

void DirectRenderer::DecideRenderPassAllocationsForFrame(
    const RenderPassList& render_passes_in_draw_order) {
  base::hash_map<RenderPass::Id, const RenderPass*> render_passes_in_frame;
  for (size_t i = 0; i < render_passes_in_draw_order.size(); ++i)
    render_passes_in_frame.insert(std::pair<RenderPass::Id, const RenderPass*>(
        render_passes_in_draw_order[i]->id, render_passes_in_draw_order[i]));

  std::vector<RenderPass::Id> passes_to_delete;
  ScopedPtrHashMap<RenderPass::Id, CachedResource>::const_iterator pass_iter;
  for (pass_iter = render_pass_textures_.begin();
       pass_iter != render_pass_textures_.end();
       ++pass_iter) {
    base::hash_map<RenderPass::Id, const RenderPass*>::const_iterator it =
        render_passes_in_frame.find(pass_iter->first);
    if (it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pass_iter->first);
      continue;
    }

    const RenderPass* render_pass_in_frame = it->second;
    gfx::Size required_size = RenderPassTextureSize(render_pass_in_frame);
    GLenum required_format = RenderPassTextureFormat(render_pass_in_frame);
    CachedResource* texture = pass_iter->second;
    DCHECK(texture);

    bool size_appropriate = texture->size().width() >= required_size.width() &&
                           texture->size().height() >= required_size.height();
    if (texture->id() &&
        (!size_appropriate || texture->format() != required_format))
      texture->Free();
  }

  // Delete RenderPass textures from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i)
    render_pass_textures_.erase(passes_to_delete[i]);

  for (size_t i = 0; i < render_passes_in_draw_order.size(); ++i) {
    if (!render_pass_textures_.contains(render_passes_in_draw_order[i]->id)) {
      scoped_ptr<CachedResource> texture =
          CachedResource::Create(resource_provider_);
      render_pass_textures_.set(render_passes_in_draw_order[i]->id,
                              texture.Pass());
    }
  }
}

void DirectRenderer::DrawFrame(RenderPassList& render_passes_in_draw_order) {
  TRACE_EVENT0("cc", "DirectRenderer::DrawFrame");
  UMA_HISTOGRAM_COUNTS("Renderer4.renderPassCount",
                       render_passes_in_draw_order.size());

  const RenderPass* root_render_pass = render_passes_in_draw_order.back();
  DCHECK(root_render_pass);

  DrawingFrame frame;
  frame.root_render_pass = root_render_pass;
  frame.root_damage_rect =
      Capabilities().using_partial_swap ?
      root_render_pass->damage_rect : root_render_pass->output_rect;
  frame.root_damage_rect.Intersect(gfx::Rect(ViewportSize()));

  BeginDrawingFrame(frame);
  for (size_t i = 0; i < render_passes_in_draw_order.size(); ++i)
    DrawRenderPass(frame, render_passes_in_draw_order[i]);
  FinishDrawingFrame(frame);

  render_passes_in_draw_order.clear();
}

gfx::RectF DirectRenderer::ComputeScissorRectForRenderPass(
    const DrawingFrame& frame) {
  gfx::RectF render_pass_scissor = frame.current_render_pass->output_rect;

  if (frame.root_damage_rect == frame.root_render_pass->output_rect)
    return render_pass_scissor;

  gfx::Transform inverse_transform(gfx::Transform::kSkipInitialization);
  if (frame.current_render_pass->transform_to_root_target.GetInverse(
          &inverse_transform)) {
    // Only intersect inverse-projected damage if the transform is invertible.
    gfx::RectF damage_rect_in_render_pass_space =
        MathUtil::ProjectClippedRect(inverse_transform, frame.root_damage_rect);
    render_pass_scissor.Intersect(damage_rect_in_render_pass_space);
  }

  return render_pass_scissor;
}

void DirectRenderer::SetScissorStateForQuad(const DrawingFrame& frame,
                                            const DrawQuad& quad) {
  if (quad.isClipped()) {
    gfx::RectF quad_scissor_rect = quad.clipRect();
    SetScissorTestRect(MoveScissorToWindowSpace(frame, quad_scissor_rect));
  } else {
    EnsureScissorTestDisabled();
  }
}

void DirectRenderer::SetScissorStateForQuadWithRenderPassScissor(
    const DrawingFrame& frame,
    const DrawQuad& quad,
    const gfx::RectF& render_pass_scissor,
    bool* should_skip_quad) {
  gfx::RectF quad_scissor_rect = render_pass_scissor;

  if (quad.isClipped())
    quad_scissor_rect.Intersect(quad.clipRect());

  if (quad_scissor_rect.IsEmpty()) {
    *should_skip_quad = true;
    return;
  }

  *should_skip_quad = false;
  SetScissorTestRect(MoveScissorToWindowSpace(frame, quad_scissor_rect));
}

void DirectRenderer::FinishDrawingQuadList() {}

void DirectRenderer::DrawRenderPass(DrawingFrame& frame,
                                    const RenderPass* render_pass) {
  TRACE_EVENT0("cc", "DirectRenderer::DrawRenderPass");
  if (!UseRenderPass(frame, render_pass))
    return;

  bool using_scissor_as_optimization = Capabilities().using_partial_swap;
  gfx::RectF render_pass_scissor;

  if (using_scissor_as_optimization) {
    render_pass_scissor = ComputeScissorRectForRenderPass(frame);
    SetScissorTestRect(MoveScissorToWindowSpace(frame, render_pass_scissor));
  }

  if (frame.current_render_pass != frame.root_render_pass ||
      client_->ShouldClearRootRenderPass()) {
    if (!using_scissor_as_optimization)
      EnsureScissorTestDisabled();
    ClearFramebuffer(frame);
  }

  const QuadList& quad_list = render_pass->quad_list;
  for (QuadList::ConstBackToFrontIterator it = quad_list.BackToFrontBegin();
       it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad& quad = *(*it);
    bool should_skip_quad = false;

    if (using_scissor_as_optimization) {
      SetScissorStateForQuadWithRenderPassScissor(
          frame, quad, render_pass_scissor, &should_skip_quad);
    } else {
      SetScissorStateForQuad(frame, quad);
    }

    if (!should_skip_quad)
      DoDrawQuad(frame, *it);
  }
  FinishDrawingQuadList();

  CachedResource* texture = render_pass_textures_.get(render_pass->id);
  if (texture) {
    texture->set_is_complete(
        !render_pass->has_occlusion_from_outside_target_surface);
  }
}

bool DirectRenderer::UseRenderPass(DrawingFrame& frame,
                                   const RenderPass* render_pass) {
  frame.current_render_pass = render_pass;
  frame.current_texture = NULL;

  if (render_pass == frame.root_render_pass) {
    BindFramebufferToOutputSurface(frame);
    InitializeMatrices(frame, render_pass->output_rect, FlippedFramebuffer());
    SetDrawViewportSize(render_pass->output_rect.size());
    return true;
  }

  CachedResource* texture = render_pass_textures_.get(render_pass->id);
  DCHECK(texture);

  gfx::Size size = RenderPassTextureSize(render_pass);
  size.Enlarge(enlarge_pass_texture_amount_.x(),
               enlarge_pass_texture_amount_.y());
  if (!texture->id() &&
      !texture->Allocate(size,
                         RenderPassTextureFormat(render_pass),
                         ResourceProvider::TextureUsageFramebuffer))
    return false;

  return BindFramebufferToTexture(frame, texture, render_pass->output_rect);
}

bool DirectRenderer::HaveCachedResourcesForRenderPassId(RenderPass::Id id)
    const {
  if (!Settings().cache_render_pass_contents)
    return false;

  CachedResource* texture = render_pass_textures_.get(id);
  return texture && texture->id() && texture->is_complete();
}

// static
gfx::Size DirectRenderer::RenderPassTextureSize(const RenderPass* render_pass) {
  return render_pass->output_rect.size();
}

// static
GLenum DirectRenderer::RenderPassTextureFormat(const RenderPass* render_pass) {
  return GL_RGBA;
}

}  // namespace cc
