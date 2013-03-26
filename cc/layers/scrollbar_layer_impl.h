// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLLBAR_LAYER_IMPL_H_
#define CC_LAYERS_SCROLLBAR_LAYER_IMPL_H_

#include "cc/base/cc_export.h"
#include "cc/layers/scrollbar_geometry_fixed_thumb.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"

namespace cc {

class ScrollView;

class CC_EXPORT ScrollbarLayerImpl : public ScrollbarLayerImplBase {
 public:
  static scoped_ptr<ScrollbarLayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      scoped_ptr<ScrollbarGeometryFixedThumb> geometry);
  virtual ~ScrollbarLayerImpl();

  virtual ScrollbarLayerImpl* ToScrollbarLayer() OVERRIDE;
  int scroll_layer_id() const { return scroll_layer_id_; }
  void set_scroll_layer_id(int id) { scroll_layer_id_ = id; }

  void SetScrollbarData(WebKit::WebScrollbar* scrollbar);
  void SetThumbSize(gfx::Size size);

  void set_vertical_adjust(float vertical_adjust) {
    vertical_adjust_ = vertical_adjust;
  }
  void SetViewportWithinScrollableArea(gfx::RectF scrollable_viewport,
      gfx::SizeF scrollable_area);

  void set_back_track_resource_id(ResourceProvider::ResourceId id) {
    back_track_resource_id_ = id;
  }
  void set_fore_track_resource_id(ResourceProvider::ResourceId id) {
    fore_track_resource_id_ = id;
  }
  void set_thumb_resource_id(ResourceProvider::ResourceId id) {
    thumb_resource_id_ = id;
  }
  bool HasThumbTexture() { return thumb_resource_id_; }


  // ScrollbarLayerImplBase implementation.
  virtual float CurrentPos() const OVERRIDE;
  virtual int TotalSize() const OVERRIDE;
  virtual int Maximum() const OVERRIDE;

  void SetCurrentPos(float current_pos) { current_pos_ = current_pos; }
  void SetTotalSize(int total_size) { total_size_ = total_size; }
  void SetMaximum(int maximum) { maximum_ = maximum; }

  virtual WebKit::WebScrollbar::Orientation Orientation() const OVERRIDE;

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  virtual void DidLoseOutputSurface() OVERRIDE;

 protected:
  ScrollbarLayerImpl(LayerTreeImpl* tree_impl,
                     int id,
                     scoped_ptr<ScrollbarGeometryFixedThumb> geometry);

 private:
  // nested class only to avoid namespace problem
  class Scrollbar : public WebKit::WebScrollbar {
   public:
    explicit Scrollbar(ScrollbarLayerImpl* owner) : owner_(owner) {}

    // WebScrollbar implementation
    virtual bool isOverlay() const;
    virtual int value() const;
    virtual WebKit::WebPoint location() const;
    virtual WebKit::WebSize size() const;
    virtual bool enabled() const;
    virtual int maximum() const;
    virtual int totalSize() const;
    virtual bool isScrollViewScrollbar() const;
    virtual bool isScrollableAreaActive() const;
    virtual void getTickmarks(WebKit::WebVector<WebKit::WebRect>& tickmarks)
        const;
    virtual WebScrollbar::ScrollbarControlSize controlSize() const;
    virtual WebScrollbar::ScrollbarPart pressedPart() const;
    virtual WebScrollbar::ScrollbarPart hoveredPart() const;
    virtual WebScrollbar::ScrollbarOverlayStyle scrollbarOverlayStyle() const;
    virtual WebScrollbar::Orientation orientation() const;
    virtual bool isCustomScrollbar() const;

   private:
    ScrollbarLayerImpl* owner_;
  };

  virtual const char* LayerTypeAsString() const OVERRIDE;

  gfx::Rect ScrollbarLayerRectToContentRect(gfx::RectF layer_rect) const;

  Scrollbar scrollbar_;

  ResourceProvider::ResourceId back_track_resource_id_;
  ResourceProvider::ResourceId fore_track_resource_id_;
  ResourceProvider::ResourceId thumb_resource_id_;

  scoped_ptr<ScrollbarGeometryFixedThumb> geometry_;

  float current_pos_;
  int total_size_;
  int maximum_;
  gfx::Size thumb_size_;

  // Difference between the clip layer's height and the visible viewport
  // height (which may differ in the presence of top-controls hiding).
  float vertical_adjust_;

  // Specifies the position and size of the viewport within the scrollable
  // area (normalized as if the scrollable area is a unit-sized box
  // [0, 0, 1, 1]).
  gfx::RectF normalized_viewport_;

  int scroll_layer_id_;

  // Data to implement Scrollbar
  WebKit::WebScrollbar::ScrollbarOverlayStyle scrollbar_overlay_style_;
  WebKit::WebVector<WebKit::WebRect> tickmarks_;
  WebKit::WebScrollbar::Orientation orientation_;
  WebKit::WebScrollbar::ScrollbarControlSize control_size_;
  WebKit::WebScrollbar::ScrollbarPart pressed_part_;
  WebKit::WebScrollbar::ScrollbarPart hovered_part_;

  bool is_scrollable_area_active_;
  bool is_scroll_view_scrollbar_;
  bool enabled_;
  bool is_custom_scrollbar_;
  bool is_overlay_scrollbar_;
};

}  // namespace cc
#endif  // CC_LAYERS_SCROLLBAR_LAYER_IMPL_H_
