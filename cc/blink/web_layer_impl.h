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
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebDoublePoint.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/core/SkMatrix44.h"

namespace cc {
class FilterOperations;
class Layer;
}

namespace cc_blink {

class CC_BLINK_EXPORT WebLayerImpl : public NON_EXPORTED_BASE(blink::WebLayer) {
 public:
  WebLayerImpl();
  explicit WebLayerImpl(scoped_refptr<cc::Layer>);
  ~WebLayerImpl() override;

  cc::Layer* layer() const;

  // If set to true, content opaqueness cannot be changed using setOpaque.
  // However, it can still be modified using SetContentsOpaque on the
  // cc::Layer.
  void SetContentsOpaqueIsFixed(bool fixed);

  // WebLayer implementation.
  int Id() const override;
  void InvalidateRect(const blink::WebRect&) override;
  void Invalidate() override;
  void AddChild(blink::WebLayer* child) override;
  void InsertChild(blink::WebLayer* child, size_t index) override;
  void ReplaceChild(blink::WebLayer* reference,
                    blink::WebLayer* new_layer) override;
  void RemoveFromParent() override;
  void RemoveAllChildren() override;
  void SetBounds(const blink::WebSize& bounds) override;
  blink::WebSize Bounds() const override;
  void SetMasksToBounds(bool masks_to_bounds) override;
  bool MasksToBounds() const override;
  void SetMaskLayer(blink::WebLayer* mask) override;
  void SetOpacity(float opacity) override;
  float Opacity() const override;
  void SetBlendMode(blink::WebBlendMode blend_mode) override;
  blink::WebBlendMode BlendMode() const override;
  void SetIsRootForIsolatedGroup(bool root) override;
  bool IsRootForIsolatedGroup() override;
  void SetShouldHitTest(bool should_hit_test) override;
  bool ShouldHitTest() override;
  void SetOpaque(bool opaque) override;
  bool Opaque() const override;
  void SetPosition(const blink::WebFloatPoint& position) override;
  blink::WebFloatPoint GetPosition() const override;
  void SetTransform(const SkMatrix44& transform) override;
  void SetTransformOrigin(const blink::WebFloatPoint3D& point) override;
  blink::WebFloatPoint3D TransformOrigin() const override;
  SkMatrix44 Transform() const override;
  void SetDrawsContent(bool draws_content) override;
  bool DrawsContent() const override;
  void SetDoubleSided(bool double_sided) override;
  void SetShouldFlattenTransform(bool flatten) override;
  void SetRenderingContext(int context) override;
  void SetUseParentBackfaceVisibility(bool visible) override;
  void SetBackgroundColor(blink::WebColor color) override;
  blink::WebColor BackgroundColor() const override;
  void SetFilters(const cc::FilterOperations& filters) override;
  void SetFiltersOrigin(const blink::WebFloatPoint& origin) override;
  void SetBackgroundFilters(const cc::FilterOperations& filters) override;
  bool HasTickingAnimationForTesting() override;
  void SetScrollPosition(blink::WebFloatPoint position) override;
  blink::WebFloatPoint ScrollPosition() const override;
  void SetScrollClipLayer(blink::WebLayer* clip_layer) override;
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
  void SetNonFastScrollableRegion(
      const blink::WebVector<blink::WebRect>& region) override;
  blink::WebVector<blink::WebRect> NonFastScrollableRegion() const override;
  void SetTouchEventHandlerRegion(
      const blink::WebVector<blink::WebTouchInfo>& touch_info) override;
  blink::WebVector<blink::WebRect> TouchEventHandlerRegion() const override;
  void SetIsContainerForFixedPositionLayers(bool is_container) override;
  bool IsContainerForFixedPositionLayers() const override;
  void SetPositionConstraint(
      const blink::WebLayerPositionConstraint& constraint) override;
  blink::WebLayerPositionConstraint PositionConstraint() const override;
  void SetStickyPositionConstraint(
      const blink::WebLayerStickyPositionConstraint& constraint) override;
  blink::WebLayerStickyPositionConstraint StickyPositionConstraint()
      const override;
  void SetScrollClient(blink::WebLayerScrollClient* client) override;
  void SetLayerClient(cc::LayerClient* client) override;
  const cc::Layer* CcLayer() const override;
  cc::Layer* CcLayer() override;
  void SetElementId(const cc::ElementId&) override;
  cc::ElementId GetElementId() const override;
  void SetCompositorMutableProperties(uint32_t properties) override;
  uint32_t CompositorMutableProperties() const override;
  void SetHasWillChangeTransformHint(bool has_will_change) override;
  void ShowScrollbars() override;

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
