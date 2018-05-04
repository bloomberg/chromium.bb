// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event_impl.h"
#include "cc/base/region.h"
#include "cc/base/switches.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/touch_action_region.h"
#include "cc/trees/element_id.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using cc::Layer;
using blink::WebLayer;

namespace cc_blink {

WebLayerImpl::WebLayerImpl()
    : layer_(Layer::Create()), contents_opaque_is_fixed_(false) {}

WebLayerImpl::WebLayerImpl(scoped_refptr<Layer> layer)
    : layer_(layer), contents_opaque_is_fixed_(false) {
}

WebLayerImpl::~WebLayerImpl() = default;

int WebLayerImpl::Id() const {
  return layer_->id();
}

DISABLE_CFI_PERF
void WebLayerImpl::InvalidateRect(const gfx::Rect& rect) {
  layer_->SetNeedsDisplayRect(rect);
}

void WebLayerImpl::Invalidate() {
  layer_->SetNeedsDisplay();
}

void WebLayerImpl::AddChild(WebLayer* child) {
  layer_->AddChild(static_cast<WebLayerImpl*>(child)->layer());
}

void WebLayerImpl::InsertChild(WebLayer* child, size_t index) {
  layer_->InsertChild(static_cast<WebLayerImpl*>(child)->layer(), index);
}

void WebLayerImpl::ReplaceChild(WebLayer* reference, WebLayer* new_layer) {
  layer_->ReplaceChild(static_cast<WebLayerImpl*>(reference)->layer(),
                       static_cast<WebLayerImpl*>(new_layer)->layer());
}

void WebLayerImpl::RemoveFromParent() {
  layer_->RemoveFromParent();
}

void WebLayerImpl::RemoveAllChildren() {
  layer_->RemoveAllChildren();
}

void WebLayerImpl::SetBounds(const gfx::Size& size) {
  layer_->SetBounds(size);
}

const gfx::Size& WebLayerImpl::Bounds() const {
  return layer_->bounds();
}

void WebLayerImpl::SetMasksToBounds(bool masks_to_bounds) {
  layer_->SetMasksToBounds(masks_to_bounds);
}

bool WebLayerImpl::MasksToBounds() const {
  return layer_->masks_to_bounds();
}

void WebLayerImpl::SetMaskLayer(WebLayer* maskLayer) {
  layer_->SetMaskLayer(
      maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : nullptr);
}

void WebLayerImpl::SetOpacity(float opacity) {
  layer_->SetOpacity(opacity);
}

float WebLayerImpl::Opacity() const {
  return layer_->opacity();
}

void WebLayerImpl::SetBlendMode(SkBlendMode blend_mode) {
  layer_->SetBlendMode(blend_mode);
}

SkBlendMode WebLayerImpl::BlendMode() const {
  return layer_->blend_mode();
}

void WebLayerImpl::SetIsRootForIsolatedGroup(bool isolate) {
  layer_->SetIsRootForIsolatedGroup(isolate);
}

bool WebLayerImpl::IsRootForIsolatedGroup() {
  return layer_->is_root_for_isolated_group();
}

void WebLayerImpl::SetHitTestableWithoutDrawsContent(bool should_hit_test) {
  layer_->SetHitTestableWithoutDrawsContent(should_hit_test);
}

void WebLayerImpl::SetOpaque(bool opaque) {
  if (contents_opaque_is_fixed_)
    return;
  layer_->SetContentsOpaque(opaque);
}

bool WebLayerImpl::Opaque() const {
  return layer_->contents_opaque();
}

void WebLayerImpl::SetPosition(const gfx::PointF& position) {
  layer_->SetPosition(position);
}

const gfx::PointF& WebLayerImpl::GetPosition() const {
  return layer_->position();
}

void WebLayerImpl::SetTransform(const gfx::Transform& transform) {
  layer_->SetTransform(transform);
}

void WebLayerImpl::SetTransformOrigin(const gfx::Point3F& point) {
  layer_->SetTransformOrigin(point);
}

const gfx::Point3F& WebLayerImpl::TransformOrigin() const {
  return layer_->transform_origin();
}

const gfx::Transform& WebLayerImpl::Transform() const {
  return layer_->transform();
}

void WebLayerImpl::SetDrawsContent(bool draws_content) {
  layer_->SetIsDrawable(draws_content);
}

bool WebLayerImpl::DrawsContent() const {
  return layer_->DrawsContent();
}

void WebLayerImpl::SetDoubleSided(bool double_sided) {
  layer_->SetDoubleSided(double_sided);
}

void WebLayerImpl::SetShouldFlattenTransform(bool flatten) {
  layer_->SetShouldFlattenTransform(flatten);
}

void WebLayerImpl::SetRenderingContext(int context) {
  layer_->Set3dSortingContextId(context);
}

void WebLayerImpl::SetUseParentBackfaceVisibility(
    bool use_parent_backface_visibility) {
  layer_->SetUseParentBackfaceVisibility(use_parent_backface_visibility);
}

void WebLayerImpl::SetBackgroundColor(SkColor color) {
  layer_->SetBackgroundColor(color);
}

SkColor WebLayerImpl::BackgroundColor() const {
  return layer_->background_color();
}

void WebLayerImpl::SetFilters(const cc::FilterOperations& filters) {
  layer_->SetFilters(filters);
}

void WebLayerImpl::SetFiltersOrigin(const gfx::PointF& origin) {
  layer_->SetFiltersOrigin(origin);
}

void WebLayerImpl::SetBackgroundFilters(const cc::FilterOperations& filters) {
  layer_->SetBackgroundFilters(filters);
}

bool WebLayerImpl::HasTickingAnimationForTesting() {
  return layer_->HasTickingAnimationForTesting();
}

void WebLayerImpl::SetScrollable(const gfx::Size& size) {
  layer_->SetScrollable(size);
}

void WebLayerImpl::SetScrollPosition(const gfx::ScrollOffset& position) {
  layer_->SetScrollOffset(position);
}

const gfx::ScrollOffset& WebLayerImpl::ScrollPosition() const {
  return layer_->scroll_offset();
}

bool WebLayerImpl::Scrollable() const {
  return layer_->scrollable();
}

const gfx::Size& WebLayerImpl::ScrollContainerBoundsForTesting() const {
  return layer_->scroll_container_bounds();
}

void WebLayerImpl::SetUserScrollable(bool horizontal, bool vertical) {
  layer_->SetUserScrollable(horizontal, vertical);
}

bool WebLayerImpl::UserScrollableHorizontal() const {
  return layer_->user_scrollable_horizontal();
}

bool WebLayerImpl::UserScrollableVertical() const {
  return layer_->user_scrollable_vertical();
}

void WebLayerImpl::AddMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons) {
  // WebLayerImpl should only know about non-transient scrolling
  // reasons. Transient scrolling reasons are computed per hit test.
  DCHECK(main_thread_scrolling_reasons);
  DCHECK(cc::MainThreadScrollingReason::MainThreadCanSetScrollReasons(
      main_thread_scrolling_reasons));
  layer_->AddMainThreadScrollingReasons(main_thread_scrolling_reasons);
}

void WebLayerImpl::ClearMainThreadScrollingReasons(
    uint32_t main_thread_scrolling_reasons_to_clear) {
  layer_->ClearMainThreadScrollingReasons(
      main_thread_scrolling_reasons_to_clear);
}

uint32_t WebLayerImpl::MainThreadScrollingReasons() {
  return layer_->main_thread_scrolling_reasons();
}

bool WebLayerImpl::ShouldScrollOnMainThread() const {
  return layer_->should_scroll_on_main_thread();
}

void WebLayerImpl::SetNonFastScrollableRegion(const cc::Region& region) {
  layer_->SetNonFastScrollableRegion(region);
}

const cc::Region& WebLayerImpl::NonFastScrollableRegion() const {
  return layer_->non_fast_scrollable_region();
}

void WebLayerImpl::SetTouchEventHandlerRegion(
    const cc::TouchActionRegion& region) {
  layer_->SetTouchActionRegion(region);
}

const cc::TouchActionRegion& WebLayerImpl::TouchEventHandlerRegion() const {
  return layer_->touch_action_region();
}

const cc::Region& WebLayerImpl::TouchEventHandlerRegionForTouchActionForTesting(
    cc::TouchAction touch_action) const {
  return layer_->touch_action_region().GetRegionForTouchAction(touch_action);
}

void WebLayerImpl::SetIsContainerForFixedPositionLayers(bool enable) {
  layer_->SetIsContainerForFixedPositionLayers(enable);
}

bool WebLayerImpl::IsContainerForFixedPositionLayers() const {
  return layer_->IsContainerForFixedPositionLayers();
}

void WebLayerImpl::SetIsResizedByBrowserControls(bool enable) {
  layer_->SetIsResizedByBrowserControls(enable);
}

void WebLayerImpl::SetPositionConstraint(
    const cc::LayerPositionConstraint& constraint) {
  layer_->SetPositionConstraint(constraint);
}

const cc::LayerPositionConstraint& WebLayerImpl::PositionConstraint() const {
  return layer_->position_constraint();
}

void WebLayerImpl::SetStickyPositionConstraint(
    const cc::LayerStickyPositionConstraint& constraint) {
  layer_->SetStickyPositionConstraint(constraint);
}

const cc::LayerStickyPositionConstraint&
WebLayerImpl::StickyPositionConstraint() const {
  return layer_->sticky_position_constraint();
}

void WebLayerImpl::SetScrollCallback(
    base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                 const cc::ElementId&)> callback) {
  layer_->set_did_scroll_callback(std::move(callback));
}

void WebLayerImpl::SetScrollOffsetFromImplSideForTesting(
    const gfx::ScrollOffset& offset) {
  layer_->SetScrollOffsetFromImplSide(offset);
}

void WebLayerImpl::SetLayerClient(base::WeakPtr<cc::LayerClient> client) {
  layer_->SetLayerClient(std::move(client));
}

const cc::Layer* WebLayerImpl::CcLayer() const {
  return layer_.get();
}

cc::Layer* WebLayerImpl::CcLayer() {
  return layer_.get();
}

void WebLayerImpl::SetElementId(const cc::ElementId& id) {
  layer_->SetElementId(id);
}

cc::ElementId WebLayerImpl::GetElementId() const {
  return layer_->element_id();
}

void WebLayerImpl::SetScrollParent(blink::WebLayer* parent) {
  cc::Layer* scroll_parent = nullptr;
  if (parent)
    scroll_parent = static_cast<WebLayerImpl*>(parent)->layer();
  layer_->SetScrollParent(scroll_parent);
}

void WebLayerImpl::SetClipParent(blink::WebLayer* parent) {
  cc::Layer* clip_parent = nullptr;
  if (parent)
    clip_parent = static_cast<WebLayerImpl*>(parent)->layer();
  layer_->SetClipParent(clip_parent);
}

Layer* WebLayerImpl::layer() const {
  return layer_.get();
}

void WebLayerImpl::SetContentsOpaqueIsFixed(bool fixed) {
  contents_opaque_is_fixed_ = fixed;
}

void WebLayerImpl::SetHasWillChangeTransformHint(bool has_will_change) {
  layer_->SetHasWillChangeTransformHint(has_will_change);
}

void WebLayerImpl::ShowScrollbars() {
  layer_->ShowScrollbars();
}

void WebLayerImpl::SetOverscrollBehavior(
    const cc::OverscrollBehavior& behavior) {
  layer_->SetOverscrollBehavior(behavior);
}

void WebLayerImpl::SetSnapContainerData(
    base::Optional<cc::SnapContainerData> data) {
  layer_->SetSnapContainerData(std::move(data));
}

}  // namespace cc_blink
