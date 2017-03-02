// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_overlay_scrollbar_layer_impl.h"

namespace cc {

std::unique_ptr<PaintedOverlayScrollbarLayerImpl>
PaintedOverlayScrollbarLayerImpl::Create(LayerTreeImpl* tree_impl,
                                         int id,
                                         ScrollbarOrientation orientation,
                                         bool is_left_side_vertical_scrollbar) {
  return base::WrapUnique(new PaintedOverlayScrollbarLayerImpl(
      tree_impl, id, orientation, is_left_side_vertical_scrollbar));
}

PaintedOverlayScrollbarLayerImpl::PaintedOverlayScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation,
    bool is_left_side_vertical_scrollbar)
    : ScrollbarLayerImplBase(tree_impl,
                             id,
                             orientation,
                             is_left_side_vertical_scrollbar,
                             true),
      thumb_ui_resource_id_(0),
      thumb_thickness_(0),
      thumb_length_(0),
      track_start_(0),
      track_length_(0) {}

PaintedOverlayScrollbarLayerImpl::~PaintedOverlayScrollbarLayerImpl() {}

std::unique_ptr<LayerImpl> PaintedOverlayScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedOverlayScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), is_left_side_vertical_scrollbar());
}

void PaintedOverlayScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerImplBase::PushPropertiesTo(layer);

  PaintedOverlayScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedOverlayScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  scrollbar_layer->SetTrackStart(track_start_);
  scrollbar_layer->SetTrackLength(track_length_);

  scrollbar_layer->SetImageBounds(image_bounds_);
  scrollbar_layer->SetAperture(aperture_);

  scrollbar_layer->set_thumb_ui_resource_id(thumb_ui_resource_id_);
}

bool PaintedOverlayScrollbarLayerImpl::WillDraw(
    DrawMode draw_mode,
    ResourceProvider* resource_provider) {
  DCHECK(draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE);
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void PaintedOverlayScrollbarLayerImpl::AppendQuads(
    RenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  // For overlay scrollbars, the border should match the inset of the aperture
  // and be symmetrical.
  gfx::Rect border(aperture_.x(), aperture_.y(), aperture_.x() * 2,
                   aperture_.y() * 2);
  gfx::Rect thumb_quad_rect(ComputeThumbQuadRect());
  gfx::Rect layer_occlusion;
  bool fill_center = true;
  bool nearest_neighbor = false;

  quad_generator_.SetLayout(image_bounds_, thumb_quad_rect.size(), aperture_,
                            border, layer_occlusion, fill_center,
                            nearest_neighbor);
  quad_generator_.CheckGeometryLimitations();

  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(render_pass, bounds(), shared_quad_state,
                        append_quads_data);

  std::vector<NinePatchGenerator::Patch> patches =
      quad_generator_.GeneratePatches();

  gfx::Vector2dF offset = thumb_quad_rect.OffsetFromOrigin();
  for (auto& patch : patches)
    patch.output_rect += offset;

  quad_generator_.AppendQuads(this, thumb_ui_resource_id_, render_pass,
                              shared_quad_state, patches);
}

void PaintedOverlayScrollbarLayerImpl::SetThumbThickness(int thumb_thickness) {
  if (thumb_thickness_ == thumb_thickness)
    return;
  thumb_thickness_ = thumb_thickness;
  NoteLayerPropertyChanged();
}

int PaintedOverlayScrollbarLayerImpl::ThumbThickness() const {
  return thumb_thickness_;
}

void PaintedOverlayScrollbarLayerImpl::SetThumbLength(int thumb_length) {
  if (thumb_length_ == thumb_length)
    return;
  thumb_length_ = thumb_length;
  NoteLayerPropertyChanged();
}

int PaintedOverlayScrollbarLayerImpl::ThumbLength() const {
  return thumb_length_;
}

void PaintedOverlayScrollbarLayerImpl::SetTrackStart(int track_start) {
  if (track_start_ == track_start)
    return;
  track_start_ = track_start;
  NoteLayerPropertyChanged();
}

int PaintedOverlayScrollbarLayerImpl::TrackStart() const {
  return track_start_;
}

void PaintedOverlayScrollbarLayerImpl::SetTrackLength(int track_length) {
  if (track_length_ == track_length)
    return;
  track_length_ = track_length;
  NoteLayerPropertyChanged();
}

void PaintedOverlayScrollbarLayerImpl::SetImageBounds(const gfx::Size& bounds) {
  if (image_bounds_ == bounds)
    return;
  image_bounds_ = bounds;
  NoteLayerPropertyChanged();
}

void PaintedOverlayScrollbarLayerImpl::SetAperture(const gfx::Rect& aperture) {
  if (aperture_ == aperture)
    return;
  aperture_ = aperture;
  NoteLayerPropertyChanged();
}

float PaintedOverlayScrollbarLayerImpl::TrackLength() const {
  return track_length_ + (orientation() == VERTICAL ? vertical_adjust() : 0);
}

bool PaintedOverlayScrollbarLayerImpl::IsThumbResizable() const {
  return false;
}

const char* PaintedOverlayScrollbarLayerImpl::LayerTypeAsString() const {
  return "cc::PaintedOverlayScrollbarLayerImpl";
}

}  // namespace cc
