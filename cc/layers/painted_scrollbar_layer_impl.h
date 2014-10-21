// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_client.h"

namespace cc {

class LayerTreeImpl;
class ScrollView;

class CC_EXPORT PaintedScrollbarLayerImpl : public ScrollbarLayerImplBase {
 public:
  static scoped_ptr<PaintedScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      ScrollbarOrientation orientation);
  ~PaintedScrollbarLayerImpl() override;

  // LayerImpl implementation.
  scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                ResourceProvider* resource_provider) override;
  void AppendQuads(RenderPass* render_pass,
                   const Occlusion& occlusion_in_content_space,
                   AppendQuadsData* append_quads_data) override;

  void SetThumbThickness(int thumb_thickness);
  void SetThumbLength(int thumb_length);
  void SetTrackStart(int track_start);
  void SetTrackLength(int track_length);

  void set_track_ui_resource_id(UIResourceId uid) {
    track_ui_resource_id_ = uid;
  }
  void set_thumb_ui_resource_id(UIResourceId uid) {
    thumb_ui_resource_id_ = uid;
  }

 protected:
  PaintedScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                            int id,
                            ScrollbarOrientation orientation);

  // ScrollbarLayerImplBase implementation.
  int ThumbThickness() const override;
  int ThumbLength() const override;
  float TrackLength() const override;
  int TrackStart() const override;
  bool IsThumbResizable() const override;

 private:
  const char* LayerTypeAsString() const override;

  UIResourceId track_ui_resource_id_;
  UIResourceId thumb_ui_resource_id_;

  int thumb_thickness_;
  int thumb_length_;
  int track_start_;
  int track_length_;

  // Difference between the clip layer's height and the visible viewport
  // height (which may differ in the presence of top-controls hiding).
  float vertical_adjust_;

  int scroll_layer_id_;

  DISALLOW_COPY_AND_ASSIGN(PaintedScrollbarLayerImpl);
};

}  // namespace cc
#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_IMPL_H_
