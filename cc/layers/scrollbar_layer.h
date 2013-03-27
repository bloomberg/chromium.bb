// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_H_
#define CC_LAYERS_SCROLLBAR_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/contents_scaling_layer.h"
#include "cc/layers/scrollbar_theme_painter.h"
#include "cc/resources/layer_updater.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {
class CachingBitmapContentLayerUpdater;
class ResourceUpdateQueue;
class Scrollbar;
class ScrollbarThemeComposite;

class CC_EXPORT ScrollbarLayer : public ContentsScalingLayer {
 public:
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  static scoped_refptr<ScrollbarLayer> Create(
      scoped_ptr<WebKit::WebScrollbar> scrollbar,
      scoped_ptr<ScrollbarThemePainter> painter,
      scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry,
      int scroll_layer_id);

  int scroll_layer_id() const { return scroll_layer_id_; }
  void SetScrollLayerId(int id);

  virtual bool OpacityCanAnimateOnImplThread() const OVERRIDE;

  WebKit::WebScrollbar::Orientation Orientation() const;

  // Layer interface
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion,
                      RenderingStats* stats) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;

  virtual ScrollbarLayer* ToScrollbarLayer() OVERRIDE;

 protected:
  ScrollbarLayer(
      scoped_ptr<WebKit::WebScrollbar> scrollbar,
      scoped_ptr<ScrollbarThemePainter> painter,
      scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry,
      int scroll_layer_id);
  virtual ~ScrollbarLayer();

 private:
  void UpdatePart(CachingBitmapContentLayerUpdater* painter,
                  LayerUpdater::Resource* resource,
                  gfx::Rect rect,
                  ResourceUpdateQueue* queue,
                  RenderingStats* stats);
  void CreateUpdaterIfNeeded();
  gfx::Rect ScrollbarLayerRectToContentRect(gfx::Rect layer_rect) const;

  bool is_dirty() const { return !dirty_rect_.IsEmpty(); }

  int MaxTextureSize();
  float ClampScaleToMaxTextureSize(float scale);

  scoped_ptr<WebKit::WebScrollbar> scrollbar_;
  scoped_ptr<ScrollbarThemePainter> painter_;
  scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry_;
  gfx::Size thumb_size_;
  int scroll_layer_id_;

  unsigned texture_format_;

  gfx::RectF dirty_rect_;

  scoped_refptr<CachingBitmapContentLayerUpdater> back_track_updater_;
  scoped_refptr<CachingBitmapContentLayerUpdater> fore_track_updater_;
  scoped_refptr<CachingBitmapContentLayerUpdater> thumb_updater_;

  // All the parts of the scrollbar except the thumb
  scoped_ptr<LayerUpdater::Resource> back_track_;
  scoped_ptr<LayerUpdater::Resource> fore_track_;
  scoped_ptr<LayerUpdater::Resource> thumb_;
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLLBAR_LAYER_H_
