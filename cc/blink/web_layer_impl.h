// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_LAYER_IMPL_H_
#define CC_BLINK_WEB_LAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/layers/layer_client.h"
#include "third_party/blink/public/platform/web_layer.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace cc {
class FilterOperations;
class Layer;
}

namespace cc_blink {

class CC_BLINK_EXPORT WebLayerImpl : public blink::WebLayer {
 public:
  WebLayerImpl();
  explicit WebLayerImpl(scoped_refptr<cc::Layer>);
  ~WebLayerImpl() override;

  cc::Layer* layer() const;

  // WebLayer implementation.
  int Id() const override;
  void InvalidateRect(const gfx::Rect&) override;
  void Invalidate() override;
  void AddChild(blink::WebLayer* child) override;
  void InsertChild(blink::WebLayer* child, size_t index) override;
  void ReplaceChild(blink::WebLayer* reference,
                    blink::WebLayer* new_layer) override;
  void RemoveFromParent() override;
  void RemoveAllChildren() override;
  void SetBounds(const gfx::Size& bounds) override;
  const gfx::Size& Bounds() const override;
  void SetMasksToBounds(bool masks_to_bounds) override;
  bool MasksToBounds() const override;
  void SetMaskLayer(blink::WebLayer* mask) override;
  void SetOpacity(float opacity) override;
  float Opacity() const override;
  void SetContentsOpaqueIsFixed(bool fixed) override;

  void SetBlendMode(blink::WebBlendMode blend_mode) override;
  blink::WebBlendMode BlendMode() const override;
  void SetIsRootForIsolatedGroup(bool root) override;
  bool IsRootForIsolatedGroup() override;
  void SetHitTestableWithoutDrawsContent(bool should_hit_test) override;
  void SetOpaque(bool opaque) override;
  bool Opaque() const override;
  void SetPosition(const gfx::PointF& position) override;
  const gfx::PointF& GetPosition() const override;
  void SetTransform(const gfx::Transform& transform) override;
  void SetTransformOrigin(const gfx::Point3F& point) override;
  const gfx::Point3F& TransformOrigin() const override;
  const gfx::Transform& Transform() const override;
  void SetDrawsContent(bool draws_content) override;
  bool DrawsContent() const override;
  void SetDoubleSided(bool double_sided) override;
  void SetShouldFlattenTransform(bool flatten) override;
  void SetRenderingContext(int context) override;
  void SetUseParentBackfaceVisibility(bool visible) override;
  void SetBackgroundColor(SkColor color) override;
  SkColor BackgroundColor() const override;
  void SetFilters(const cc::FilterOperations& filters) override;
  void SetFiltersOrigin(const gfx::PointF& origin) override;
  void SetBackgroundFilters(const cc::FilterOperations& filters) override;
  bool HasTickingAnimationForTesting() override;
  void SetScrollable(const gfx::Size&) override;
  const gfx::Size& ScrollContainerBoundsForTesting() const override;
  void SetScrollPosition(const gfx::ScrollOffset& position) override;
  const gfx::ScrollOffset& ScrollPosition() const override;
  bool Scrollable() const override;
  void SetUserScrollable(bool horizontal, bool vertical) override;
  bool UserScrollableHorizontal() const override;
  bool UserScrollableVertical() const override;
  void AddMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons) override;
  void ClearMainThreadScrollingReasons(
      uint32_t main_thread_scrolling_reasons_to_clear) override;
  uint32_t MainThreadScrollingReasons() override;
  bool ShouldScrollOnMainThread() const override;
  void SetNonFastScrollableRegion(const cc::Region& region) override;
  const cc::Region& NonFastScrollableRegion() const override;
  void SetTouchEventHandlerRegion(const cc::TouchActionRegion& region) override;
  const cc::TouchActionRegion& TouchEventHandlerRegion() const override;
  const cc::Region& TouchEventHandlerRegionForTouchActionForTesting(
      cc::TouchAction) const override;
  void SetIsContainerForFixedPositionLayers(bool is_container) override;
  bool IsContainerForFixedPositionLayers() const override;
  void SetIsResizedByBrowserControls(bool) override;
  void SetPositionConstraint(
      const cc::LayerPositionConstraint& constraint) override;
  const cc::LayerPositionConstraint& PositionConstraint() const override;
  void SetStickyPositionConstraint(
      const cc::LayerStickyPositionConstraint& constraint) override;
  const cc::LayerStickyPositionConstraint& StickyPositionConstraint()
      const override;
  void SetScrollClient(blink::WebLayerScrollClient* client) override;
  void SetScrollOffsetFromImplSideForTesting(const gfx::ScrollOffset&) override;
  void SetLayerClient(base::WeakPtr<cc::LayerClient> client) override;
  const cc::Layer* CcLayer() const override;
  cc::Layer* CcLayer() override;
  void SetElementId(const cc::ElementId&) override;
  cc::ElementId GetElementId() const override;
  void SetHasWillChangeTransformHint(bool has_will_change) override;
  void ShowScrollbars() override;
  void SetOverscrollBehavior(const cc::OverscrollBehavior&) override;
  void SetSnapContainerData(base::Optional<cc::SnapContainerData>) override;

  void SetScrollParent(blink::WebLayer* parent) override;
  void SetClipParent(blink::WebLayer* parent) override;

 protected:
  scoped_refptr<cc::Layer> layer_;

  bool contents_opaque_is_fixed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_LAYER_IMPL_H_
