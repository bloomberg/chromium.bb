// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/compositor_state_deserializer.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/blimp/compositor_state_deserializer_client.h"
#include "cc/blimp/layer_factory.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/proto/skia_conversions.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {
namespace {

class DefaultLayerFactory : public LayerFactory {
 public:
  DefaultLayerFactory() = default;
  ~DefaultLayerFactory() override = default;

  // LayerFactory implementation.
  scoped_refptr<Layer> CreateLayer(int engine_layer_id) override {
    return Layer::Create();
  }
};

}  // namespace

CompositorStateDeserializer::CompositorStateDeserializer(
    LayerTreeHost* layer_tree_host,
    ScrollCallback scroll_callback,
    CompositorStateDeserializerClient* client)
    : layer_factory_(base::MakeUnique<DefaultLayerFactory>()),
      layer_tree_host_(layer_tree_host),
      scroll_callback_(scroll_callback),
      client_(client) {
  DCHECK(layer_tree_host_);
  DCHECK(client_);
}

CompositorStateDeserializer::~CompositorStateDeserializer() = default;

Layer* CompositorStateDeserializer::GetLayerForEngineId(
    int engine_layer_id) const {
  EngineIdToLayerMap::const_iterator layer_it =
      engine_id_to_layer_.find(engine_layer_id);
  return layer_it != engine_id_to_layer_.end() ? layer_it->second.get()
                                               : nullptr;
}

void CompositorStateDeserializer::DeserializeCompositorUpdate(
    const proto::LayerTreeHost& layer_tree_host_proto) {
  SychronizeLayerTreeState(layer_tree_host_proto.layer_tree());

  const proto::LayerUpdate& layer_updates =
      layer_tree_host_proto.layer_updates();
  for (int i = 0; i < layer_updates.layers_size(); ++i) {
    SynchronizeLayerState(layer_updates.layers(i));
  }
}

void CompositorStateDeserializer::SetLayerFactoryForTesting(
    std::unique_ptr<LayerFactory> layer_factory) {
  layer_factory_ = std::move(layer_factory);
}

void CompositorStateDeserializer::SychronizeLayerTreeState(
    const proto::LayerTree& layer_tree_proto) {
  LayerTree* layer_tree = layer_tree_host_->GetLayerTree();

  // Synchronize the tree hierarchy first.
  // TODO(khushalsagar): Don't do this if the hierarchy didn't change. See
  // crbug.com/605170.
  EngineIdToLayerMap new_engine_id_to_layer;
  if (layer_tree_proto.has_root_layer()) {
    const proto::LayerNode& root_layer_node = layer_tree_proto.root_layer();
    layer_tree->SetRootLayer(
        GetLayerAndAddToNewMap(root_layer_node, &new_engine_id_to_layer));
    SynchronizeLayerHierarchyRecursive(
        layer_tree->root_layer(), root_layer_node, &new_engine_id_to_layer);
  } else {
    layer_tree->SetRootLayer(nullptr);
  }
  engine_id_to_layer_.swap(new_engine_id_to_layer);

  // Synchronize rest of the tree state.
  layer_tree->RegisterViewportLayers(
      GetLayer(layer_tree_proto.overscroll_elasticity_layer_id()),
      GetLayer(layer_tree_proto.page_scale_layer_id()),
      GetLayer(layer_tree_proto.inner_viewport_scroll_layer_id()),
      GetLayer(layer_tree_proto.outer_viewport_scroll_layer_id()));

  layer_tree->SetDeviceScaleFactor(layer_tree_proto.device_scale_factor());
  layer_tree->SetPaintedDeviceScaleFactor(
      layer_tree_proto.painted_device_scale_factor());

  float min_page_scale_factor = layer_tree_proto.min_page_scale_factor();
  float max_page_scale_factor = layer_tree_proto.max_page_scale_factor();
  float page_scale_factor = layer_tree_proto.page_scale_factor();
  if (client_->ShouldRetainClientPageScale(page_scale_factor))
    page_scale_factor = layer_tree->page_scale_factor();
  layer_tree->SetPageScaleFactorAndLimits(
      page_scale_factor, min_page_scale_factor, max_page_scale_factor);

  layer_tree->set_background_color(layer_tree_proto.background_color());
  layer_tree->set_has_transparent_background(
      layer_tree_proto.has_transparent_background());

  LayerSelection selection;
  LayerSelectionFromProtobuf(&selection, layer_tree_proto.selection());
  layer_tree->RegisterSelection(selection);
  layer_tree->SetViewportSize(
      ProtoToSize(layer_tree_proto.device_viewport_size()));

  layer_tree->SetHaveScrollEventHandlers(
      layer_tree_proto.have_scroll_event_handlers());
  layer_tree->SetEventListenerProperties(
      EventListenerClass::kMouseWheel,
      static_cast<EventListenerProperties>(
          layer_tree_proto.wheel_event_listener_properties()));
  layer_tree->SetEventListenerProperties(
      EventListenerClass::kTouchStartOrMove,
      static_cast<EventListenerProperties>(
          layer_tree_proto.touch_start_or_move_event_listener_properties()));
  layer_tree->SetEventListenerProperties(
      EventListenerClass::kTouchEndOrCancel,
      static_cast<EventListenerProperties>(
          layer_tree_proto.touch_end_or_cancel_event_listener_properties()));
}

void CompositorStateDeserializer::SynchronizeLayerState(
    const proto::LayerProperties& layer_properties_proto) {
  int engine_layer_id = layer_properties_proto.id();
  Layer* layer = GetLayerForEngineId(engine_layer_id);

  const proto::BaseLayerProperties& base = layer_properties_proto.base();

  layer->SetNeedsDisplayRect(ProtoToRect(base.update_rect()));
  layer->SetBounds(ProtoToSize(base.bounds()));
  layer->SetMasksToBounds(base.masks_to_bounds());
  layer->SetOpacity(base.opacity());
  layer->SetBlendMode(SkXfermodeModeFromProto(base.blend_mode()));
  layer->SetIsRootForIsolatedGroup(base.is_root_for_isolated_group());
  layer->SetContentsOpaque(base.contents_opaque());
  layer->SetPosition(ProtoToPointF(base.position()));
  layer->SetTransform(ProtoToTransform(base.transform()));
  layer->SetTransformOrigin(ProtoToPoint3F(base.transform_origin()));
  layer->SetIsDrawable(base.is_drawable());
  layer->SetDoubleSided(base.double_sided());
  layer->SetShouldFlattenTransform(base.should_flatten_transform());
  layer->Set3dSortingContextId(base.sorting_context_id());
  layer->SetUseParentBackfaceVisibility(base.use_parent_backface_visibility());
  layer->SetBackgroundColor(base.background_color());

  gfx::ScrollOffset scroll_offset = ProtoToScrollOffset(base.scroll_offset());
  if (client_->ShouldRetainClientScroll(engine_layer_id, scroll_offset))
    scroll_offset = layer->scroll_offset();
  layer->SetScrollOffset(scroll_offset);

  layer->SetScrollClipLayerId(
      GetClientIdFromEngineId(base.scroll_clip_layer_id()));
  layer->SetUserScrollable(base.user_scrollable_horizontal(),
                           base.user_scrollable_vertical());

  if (layer->main_thread_scrolling_reasons()) {
    layer->ClearMainThreadScrollingReasons(
        layer->main_thread_scrolling_reasons());
  }
  if (base.main_thread_scrolling_reasons()) {
    layer->AddMainThreadScrollingReasons(base.main_thread_scrolling_reasons());
  }

  layer->SetNonFastScrollableRegion(
      RegionFromProto(base.non_fast_scrollable_region()));
  layer->SetTouchEventHandlerRegion(
      RegionFromProto(base.touch_event_handler_region()));

  layer->SetIsContainerForFixedPositionLayers(
      base.is_container_for_fixed_position_layers());

  LayerPositionConstraint position_constraint;
  position_constraint.FromProtobuf(base.position_constraint());
  layer->SetPositionConstraint(position_constraint);

  LayerStickyPositionConstraint sticky_position_constraint;
  sticky_position_constraint.FromProtobuf(base.sticky_position_constraint());
  layer->SetStickyPositionConstraint(sticky_position_constraint);
  layer->SetScrollParent(GetLayerForEngineId(base.scroll_parent_id()));
  layer->SetClipParent(GetLayerForEngineId(base.clip_parent_id()));

  layer->SetHasWillChangeTransformHint(base.has_will_change_transform_hint());
  layer->SetHideLayerAndSubtree(base.hide_layer_and_subtree());
}

void CompositorStateDeserializer::SynchronizeLayerHierarchyRecursive(
    Layer* layer,
    const proto::LayerNode& layer_node,
    EngineIdToLayerMap* new_layer_map) {
  layer->RemoveAllChildren();

  // Children.
  for (int i = 0; i < layer_node.children_size(); i++) {
    const proto::LayerNode& child_layer_node = layer_node.children(i);
    scoped_refptr<Layer> child_layer =
        GetLayerAndAddToNewMap(child_layer_node, new_layer_map);
    layer->AddChild(child_layer);
    SynchronizeLayerHierarchyRecursive(child_layer.get(), child_layer_node,
                                       new_layer_map);
  }

  // Mask Layer.
  if (layer_node.has_mask_layer()) {
    const proto::LayerNode& mask_layer_node = layer_node.mask_layer();
    scoped_refptr<Layer> mask_layer =
        GetLayerAndAddToNewMap(mask_layer_node, new_layer_map);
    layer->SetMaskLayer(mask_layer.get());
    SynchronizeLayerHierarchyRecursive(mask_layer.get(), mask_layer_node,
                                       new_layer_map);
  } else {
    layer->SetMaskLayer(nullptr);
  }

  // Scroll callback.
  layer->set_did_scroll_callback(base::Bind(scroll_callback_, layer_node.id()));
}

scoped_refptr<Layer> CompositorStateDeserializer::GetLayerAndAddToNewMap(
    const proto::LayerNode& layer_node,
    EngineIdToLayerMap* new_layer_map) {
  DCHECK(new_layer_map->find(layer_node.id()) == new_layer_map->end())
      << "A LayerNode should have been de-serialized only once";

  EngineIdToLayerMap::iterator layer_map_it =
      engine_id_to_layer_.find(layer_node.id());

  if (layer_map_it != engine_id_to_layer_.end()) {
    // We can re-use the old layer.
    (*new_layer_map)[layer_node.id()] = layer_map_it->second;
    return layer_map_it->second;
  }

  // We need to create a new layer.
  scoped_refptr<Layer> layer;
  switch (layer_node.type()) {
    case proto::LayerNode::UNKNOWN:
      NOTREACHED() << "Unknown Layer type";
    case proto::LayerNode::LAYER:
      layer = layer_factory_->CreateLayer(layer_node.id());
      break;
    default:
      // TODO(khushalsagar): Add other Layer types.
      NOTREACHED();
  }

  (*new_layer_map)[layer_node.id()] = layer;
  return layer;
}

int CompositorStateDeserializer::GetClientIdFromEngineId(int engine_layer_id) {
  Layer* layer = GetLayerForEngineId(engine_layer_id);
  return layer ? layer->id() : Layer::LayerIdLabels::INVALID_ID;
}

scoped_refptr<Layer> CompositorStateDeserializer::GetLayer(
    int engine_layer_id) {
  EngineIdToLayerMap::const_iterator layer_it =
      engine_id_to_layer_.find(engine_layer_id);
  return layer_it != engine_id_to_layer_.end() ? layer_it->second : nullptr;
}

}  // namespace cc
