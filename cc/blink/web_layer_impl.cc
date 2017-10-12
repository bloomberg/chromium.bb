// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event_impl.h"
#include "cc/base/region.h"
#include "cc/base/switches.h"
#include "cc/blink/web_blend_mode.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/touch_action_region.h"
#include "cc/trees/element_id.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebLayerPositionConstraint.h"
#include "third_party/WebKit/public/platform/WebLayerScrollClient.h"
#include "third_party/WebKit/public/platform/WebLayerStickyPositionConstraint.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

using cc::Layer;
using blink::WebLayer;
using blink::WebFloatPoint;
using blink::WebVector;
using blink::WebRect;
using blink::WebSize;
using blink::WebColor;
using blink::WebScrollBoundaryBehavior;

namespace cc_blink {

WebLayerImpl::WebLayerImpl()
    : layer_(Layer::Create()), contents_opaque_is_fixed_(false) {}

WebLayerImpl::WebLayerImpl(scoped_refptr<Layer> layer)
    : layer_(layer), contents_opaque_is_fixed_(false) {
}

WebLayerImpl::~WebLayerImpl() {
  layer_->SetLayerClient(nullptr);
}

int WebLayerImpl::Id() const {
  return layer_->id();
}

DISABLE_CFI_PERF
void WebLayerImpl::InvalidateRect(const blink::WebRect& rect) {
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

void WebLayerImpl::SetBounds(const WebSize& size) {
  layer_->SetBounds(size);
}

WebSize WebLayerImpl::Bounds() const {
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
      maskLayer ? static_cast<WebLayerImpl*>(maskLayer)->layer() : 0);
}

void WebLayerImpl::SetOpacity(float opacity) {
  layer_->SetOpacity(opacity);
}

float WebLayerImpl::Opacity() const {
  return layer_->opacity();
}

void WebLayerImpl::SetBlendMode(blink::WebBlendMode blend_mode) {
  layer_->SetBlendMode(BlendModeToSkia(blend_mode));
}

blink::WebBlendMode WebLayerImpl::BlendMode() const {
  return BlendModeFromSkia(layer_->blend_mode());
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

void WebLayerImpl::SetPosition(const WebFloatPoint& position) {
  layer_->SetPosition(position);
}

WebFloatPoint WebLayerImpl::GetPosition() const {
  return layer_->position();
}

void WebLayerImpl::SetTransform(const SkMatrix44& matrix) {
  gfx::Transform transform;
  transform.matrix() = matrix;
  layer_->SetTransform(transform);
}

void WebLayerImpl::SetTransformOrigin(const blink::WebFloatPoint3D& point) {
  gfx::Point3F gfx_point = point;
  layer_->SetTransformOrigin(gfx_point);
}

blink::WebFloatPoint3D WebLayerImpl::TransformOrigin() const {
  return layer_->transform_origin();
}

SkMatrix44 WebLayerImpl::Transform() const {
  return layer_->transform().matrix();
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

void WebLayerImpl::SetBackgroundColor(WebColor color) {
  layer_->SetBackgroundColor(color);
}

WebColor WebLayerImpl::BackgroundColor() const {
  return layer_->background_color();
}

void WebLayerImpl::SetFilters(const cc::FilterOperations& filters) {
  layer_->SetFilters(filters);
}

void WebLayerImpl::SetFiltersOrigin(const blink::WebFloatPoint& origin) {
  layer_->SetFiltersOrigin(origin);
}

void WebLayerImpl::SetBackgroundFilters(const cc::FilterOperations& filters) {
  layer_->SetBackgroundFilters(filters);
}

bool WebLayerImpl::HasTickingAnimationForTesting() {
  return layer_->HasTickingAnimationForTesting();
}

void WebLayerImpl::SetScrollable(const blink::WebSize& size) {
  layer_->SetScrollable(size);
}

void WebLayerImpl::SetScrollPosition(blink::WebFloatPoint position) {
  layer_->SetScrollOffset(gfx::ScrollOffset(position.x, position.y));
}

blink::WebFloatPoint WebLayerImpl::ScrollPosition() const {
  return blink::WebFloatPoint(layer_->scroll_offset().x(),
                              layer_->scroll_offset().y());
}

bool WebLayerImpl::Scrollable() const {
  return layer_->scrollable();
}

blink::WebSize WebLayerImpl::ScrollContainerBoundsForTesting() const {
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

void WebLayerImpl::SetNonFastScrollableRegion(const WebVector<WebRect>& rects) {
  cc::Region region;
  for (size_t i = 0; i < rects.size(); ++i)
    region.Union(rects[i]);
  layer_->SetNonFastScrollableRegion(region);
}

WebVector<WebRect> WebLayerImpl::NonFastScrollableRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(layer_->non_fast_scrollable_region());
       region_rects.has_rect();
       region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(layer_->non_fast_scrollable_region());
       region_rects.has_rect();
       region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

void WebLayerImpl::SetTouchEventHandlerRegion(
    const WebVector<blink::WebTouchInfo>& touch_info) {
  base::flat_map<blink::WebTouchAction, cc::Region> region_map;
  for (size_t i = 0; i < touch_info.size(); ++i)
    region_map[touch_info[i].touch_action].Union(touch_info[i].rect);
  layer_->SetTouchActionRegion(cc::TouchActionRegion(region_map));
}

WebVector<WebRect> WebLayerImpl::TouchEventHandlerRegion() const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(
           layer_->touch_action_region().region());
       region_rects.has_rect(); region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(
           layer_->touch_action_region().region());
       region_rects.has_rect(); region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
}

WebVector<WebRect>
WebLayerImpl::TouchEventHandlerRegionForTouchActionForTesting(
    cc::TouchAction touch_action) const {
  size_t num_rects = 0;
  for (cc::Region::Iterator region_rects(
           layer_->touch_action_region().GetRegionForTouchAction(touch_action));
       region_rects.has_rect(); region_rects.next())
    ++num_rects;

  WebVector<WebRect> result(num_rects);
  size_t i = 0;
  for (cc::Region::Iterator region_rects(
           layer_->touch_action_region().GetRegionForTouchAction(touch_action));
       region_rects.has_rect(); region_rects.next()) {
    result[i] = region_rects.rect();
    ++i;
  }
  return result;
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

static blink::WebLayerPositionConstraint ToWebLayerPositionConstraint(
    const cc::LayerPositionConstraint& constraint) {
  blink::WebLayerPositionConstraint web_constraint;
  web_constraint.is_fixed_position = constraint.is_fixed_position();
  web_constraint.is_fixed_to_right_edge = constraint.is_fixed_to_right_edge();
  web_constraint.is_fixed_to_bottom_edge = constraint.is_fixed_to_bottom_edge();
  return web_constraint;
}

static cc::LayerPositionConstraint ToLayerPositionConstraint(
    const blink::WebLayerPositionConstraint& web_constraint) {
  cc::LayerPositionConstraint constraint;
  constraint.set_is_fixed_position(web_constraint.is_fixed_position);
  constraint.set_is_fixed_to_right_edge(web_constraint.is_fixed_to_right_edge);
  constraint.set_is_fixed_to_bottom_edge(
      web_constraint.is_fixed_to_bottom_edge);
  return constraint;
}

void WebLayerImpl::SetPositionConstraint(
    const blink::WebLayerPositionConstraint& constraint) {
  layer_->SetPositionConstraint(ToLayerPositionConstraint(constraint));
}

blink::WebLayerPositionConstraint WebLayerImpl::PositionConstraint() const {
  return ToWebLayerPositionConstraint(layer_->position_constraint());
}

static blink::WebLayerStickyPositionConstraint
ToWebLayerStickyPositionConstraint(
    const cc::LayerStickyPositionConstraint& constraint) {
  blink::WebLayerStickyPositionConstraint web_constraint;
  web_constraint.is_sticky = constraint.is_sticky;
  web_constraint.is_anchored_left = constraint.is_anchored_left;
  web_constraint.is_anchored_right = constraint.is_anchored_right;
  web_constraint.is_anchored_top = constraint.is_anchored_top;
  web_constraint.is_anchored_bottom = constraint.is_anchored_bottom;
  web_constraint.left_offset = constraint.left_offset;
  web_constraint.right_offset = constraint.right_offset;
  web_constraint.top_offset = constraint.top_offset;
  web_constraint.bottom_offset = constraint.bottom_offset;
  web_constraint.scroll_container_relative_sticky_box_rect =
      constraint.scroll_container_relative_sticky_box_rect;
  web_constraint.scroll_container_relative_containing_block_rect =
      constraint.scroll_container_relative_containing_block_rect;
  web_constraint.nearest_element_shifting_sticky_box =
      constraint.nearest_element_shifting_sticky_box;
  web_constraint.nearest_element_shifting_containing_block =
      constraint.nearest_element_shifting_containing_block;
  return web_constraint;
}
static cc::LayerStickyPositionConstraint ToStickyPositionConstraint(
    const blink::WebLayerStickyPositionConstraint& web_constraint) {
  cc::LayerStickyPositionConstraint constraint;
  constraint.is_sticky = web_constraint.is_sticky;
  constraint.is_anchored_left = web_constraint.is_anchored_left;
  constraint.is_anchored_right = web_constraint.is_anchored_right;
  constraint.is_anchored_top = web_constraint.is_anchored_top;
  constraint.is_anchored_bottom = web_constraint.is_anchored_bottom;
  constraint.left_offset = web_constraint.left_offset;
  constraint.right_offset = web_constraint.right_offset;
  constraint.top_offset = web_constraint.top_offset;
  constraint.bottom_offset = web_constraint.bottom_offset;
  constraint.scroll_container_relative_sticky_box_rect =
      web_constraint.scroll_container_relative_sticky_box_rect;
  constraint.scroll_container_relative_containing_block_rect =
      web_constraint.scroll_container_relative_containing_block_rect;
  constraint.nearest_element_shifting_sticky_box =
      web_constraint.nearest_element_shifting_sticky_box;
  constraint.nearest_element_shifting_containing_block =
      web_constraint.nearest_element_shifting_containing_block;
  return constraint;
}
void WebLayerImpl::SetStickyPositionConstraint(
    const blink::WebLayerStickyPositionConstraint& constraint) {
  layer_->SetStickyPositionConstraint(ToStickyPositionConstraint(constraint));
}
blink::WebLayerStickyPositionConstraint WebLayerImpl::StickyPositionConstraint()
    const {
  return ToWebLayerStickyPositionConstraint(
      layer_->sticky_position_constraint());
}

void WebLayerImpl::SetScrollClient(blink::WebLayerScrollClient* scroll_client) {
  if (scroll_client) {
    layer_->set_did_scroll_callback(
        base::Bind(&blink::WebLayerScrollClient::DidScroll,
                   base::Unretained(scroll_client)));
  } else {
    layer_->set_did_scroll_callback(
        base::Callback<void(const gfx::ScrollOffset&, const cc::ElementId&)>());
  }
}

void WebLayerImpl::SetScrollOffsetFromImplSideForTesting(
    const gfx::ScrollOffset& offset) {
  layer_->SetScrollOffsetFromImplSide(offset);
}

void WebLayerImpl::SetLayerClient(cc::LayerClient* client) {
  layer_->SetLayerClient(client);
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

void WebLayerImpl::SetScrollBoundaryBehavior(
    const blink::WebScrollBoundaryBehavior& behavior) {
  layer_->SetScrollBoundaryBehavior(
      static_cast<cc::ScrollBoundaryBehavior>(behavior));
}

}  // namespace cc_blink
