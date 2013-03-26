// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_OVERDRAW_METRICS_H_
#define CC_DEBUG_OVERDRAW_METRICS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Rect;
class Transform;
}

namespace cc {
class LayerTreeHost;
class LayerTreeHostImpl;

class OverdrawMetrics {
 public:
  static scoped_ptr<OverdrawMetrics> Create(bool record_metrics_for_frame) {
    return make_scoped_ptr(new OverdrawMetrics(record_metrics_for_frame));
  }

  // These methods are used for saving metrics during update/commit.

  // Record pixels painted by WebKit into the texture updater, but does not mean
  // the pixels were rasterized in main memory.
  void DidPaint(gfx::Rect painted_rect);
  // Records that an invalid tile was culled and did not need to be
  // painted/uploaded, and did not contribute to other tiles needing to be
  // painted.
  void DidCullTilesForUpload(int count);
  // Records pixels that were uploaded to texture memory.
  void DidUpload(const gfx::Transform& transform_to_target,
                 gfx::Rect upload_rect,
                 gfx::Rect opaque_rect);
  // Record contents texture(s) behind present using the given number of bytes.
  void DidUseContentsTextureMemoryBytes(size_t contents_texture_use_bytes);
  // Record RenderSurfaceImpl texture(s) being present using the given number of
  // bytes.
  void DidUseRenderSurfaceTextureMemoryBytes(size_t render_surface_use_bytes);

  // These methods are used for saving metrics during draw.

  // Record pixels that were not drawn to screen.
  void DidCullForDrawing(const gfx::Transform& transform_to_target,
                         gfx::Rect before_cull_rect,
                         gfx::Rect after_cull_rect);
  // Record pixels that were drawn to screen.
  void DidDraw(const gfx::Transform& transform_to_target,
               gfx::Rect after_cull_rect,
               gfx::Rect opaque_rect);

  void RecordMetrics(const LayerTreeHost* layer_tree_host) const;
  void RecordMetrics(const LayerTreeHostImpl* layer_tree_host_impl) const;

  // Accessors for tests.
  float pixels_drawn_opaque() const { return pixels_drawn_opaque_; }
  float pixels_drawn_translucent() const { return pixels_drawn_translucent_; }
  float pixels_culled_for_drawing() const { return pixels_culled_for_drawing_; }
  float pixels_painted() const { return pixels_painted_; }
  float pixels_uploaded_opaque() const { return pixels_uploaded_opaque_; }
  float pixels_uploaded_translucent() const {
    return pixels_uploaded_translucent_;
  }
  int tiles_culled_for_upload() const { return tiles_culled_for_upload_; }

 private:
  enum MetricsType {
    UpdateAndCommit,
    DrawingToScreen
  };

  explicit OverdrawMetrics(bool record_metrics_for_frame);

  template <typename LayerTreeHostType>
  void RecordMetricsInternal(MetricsType metrics_type,
                             const LayerTreeHostType* layer_tree_host) const;

  // When false this class is a giant no-op.
  bool record_metrics_for_frame_;

  // These values are used for saving metrics during update/commit.

  // Count of pixels that were painted due to invalidation.
  float pixels_painted_;
  // Count of pixels uploaded to textures and known to be opaque.
  float pixels_uploaded_opaque_;
  // Count of pixels uploaded to textures and not known to be opaque.
  float pixels_uploaded_translucent_;
  // Count of tiles that were invalidated but not uploaded.
  int tiles_culled_for_upload_;
  // Count the number of bytes in contents textures.
  uint64 contents_texture_use_bytes_;
  // Count the number of bytes in RenderSurfaceImpl textures.
  uint64 render_surface_texture_use_bytes_;

  // These values are used for saving metrics during draw.

  // Count of pixels that are opaque (and thus occlude). Ideally this is no more
  // than wiewport width x height.
  float pixels_drawn_opaque_;
  // Count of pixels that are possibly translucent, and cannot occlude.
  float pixels_drawn_translucent_;
  // Count of pixels not drawn as they are occluded by somthing opaque.
  float pixels_culled_for_drawing_;

  DISALLOW_COPY_AND_ASSIGN(OverdrawMetrics);
};

}  // namespace cc

#endif  // CC_DEBUG_OVERDRAW_METRICS_H_
