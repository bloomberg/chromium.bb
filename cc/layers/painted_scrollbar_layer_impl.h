// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class LayerTreeImpl;
class ScrollView;

class CC_EXPORT PaintedScrollbarLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<PaintedScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation);
  virtual ~PaintedScrollbarLayerImpl();

  // LayerImpl implementation.
  virtual PaintedScrollbarLayerImpl* ToScrollbarLayer() OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  int scroll_layer_id() const { return scroll_layer_id_; }
  void set_scroll_layer_id(int id) { scroll_layer_id_ = id; }

  ScrollbarOrientation Orientation() const;
  float CurrentPos() const;
  int Maximum() const;

  void SetThumbThickness(int thumb_thickness);
  int thumb_thickness() const { return thumb_thickness_; }
  void SetThumbLength(int thumb_length);
  void SetTrackStart(int track_start);
  void SetTrackLength(int track_length);
  void SetVerticalAdjust(float vertical_adjust);
  void set_track_ui_resource_id(UIResourceId uid) {
    track_ui_resource_id_ = uid;
  }
  void set_thumb_ui_resource_id(UIResourceId uid) {
    thumb_ui_resource_id_ = uid;
  }
  void SetVisibleToTotalLengthRatio(float ratio);
  void set_is_overlay_scrollbar(bool is_overlay_scrollbar) {
    is_overlay_scrollbar_ = is_overlay_scrollbar;
  }
  bool is_overlay_scrollbar() const { return is_overlay_scrollbar_; }

  void SetCurrentPos(float current_pos);
  void SetMaximum(int maximum);

  gfx::Rect ComputeThumbQuadRect() const;

 protected:
  PaintedScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                            int id,
                            ScrollbarOrientation orientation);

 private:
  virtual const char* LayerTypeAsString() const OVERRIDE;

  gfx::Rect ScrollbarLayerRectToContentRect(gfx::RectF layer_rect) const;

  UIResourceId track_ui_resource_id_;
  UIResourceId thumb_ui_resource_id_;

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

  bool is_overlay_scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(PaintedScrollbarLayerImpl);
};

}  // namespace cc
#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
