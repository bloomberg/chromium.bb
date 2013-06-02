// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_SCROLLBAR_LAYER_IMPL_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"

namespace cc {

class LayerTreeImpl;
class ScrollView;

class CC_EXPORT ScrollbarLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<ScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation);
  virtual ~ScrollbarLayerImpl();

  // LayerImpl implementation.
  virtual ScrollbarLayerImpl* ToScrollbarLayer() OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  virtual void DidLoseOutputSurface() OVERRIDE;

  int scroll_layer_id() const { return scroll_layer_id_; }
  void set_scroll_layer_id(int id) { scroll_layer_id_ = id; }

  ScrollbarOrientation Orientation() const;
  float CurrentPos() const;
  int Maximum() const;

  void set_thumb_thickness(int thumb_thickness) {
    thumb_thickness_ = thumb_thickness;
  }
  void set_thumb_length(int thumb_length) {
    thumb_length_ = thumb_length;
  }
  void set_track_start(int track_start) {
    track_start_ = track_start;
  }
  void set_track_length(int track_length) {
    track_length_ = track_length;
  }
  void set_vertical_adjust(float vertical_adjust) {
    vertical_adjust_ = vertical_adjust;
  }
  void set_track_resource_id(ResourceProvider::ResourceId id) {
    track_resource_id_ = id;
  }
  void set_thumb_resource_id(ResourceProvider::ResourceId id) {
    thumb_resource_id_ = id;
  }
  void set_visible_to_total_length_ratio(float ratio) {
    visible_to_total_length_ratio_ = ratio;
  }

  void SetCurrentPos(float current_pos) { current_pos_ = current_pos; }
  void SetMaximum(int maximum) { maximum_ = maximum; }

  gfx::Rect ComputeThumbQuadRect() const;

 protected:
  ScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                     int id,
                     ScrollbarOrientation orientation);

 private:
  virtual const char* LayerTypeAsString() const OVERRIDE;

  gfx::Rect ScrollbarLayerRectToContentRect(gfx::RectF layer_rect) const;

  ResourceProvider::ResourceId track_resource_id_;
  ResourceProvider::ResourceId thumb_resource_id_;

  float current_pos_;
  int maximum_;
  int thumb_thickness_;
  int thumb_length_;
  int track_start_;
  int track_length_;
  ScrollbarOrientation orientation_;

  // Difference between the clip layer's height and the visible viewport
  // height (which may differ in the presence of top-controls hiding).
  float vertical_adjust_;

  float visible_to_total_length_ratio_;

  int scroll_layer_id_;

  bool is_scrollable_area_active_;
  bool is_scroll_view_scrollbar_;
  bool enabled_;
  bool is_custom_scrollbar_;
  bool is_overlay_scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarLayerImpl);
};

}  // namespace cc
#endif  // CC_LAYERS_SCROLLBAR_LAYER_IMPL_H_
