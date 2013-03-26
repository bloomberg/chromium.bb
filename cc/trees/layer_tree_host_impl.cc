// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/debug_rect_history.h"
#include "cc/debug/frame_rate_counter.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/debug/paint_time_counter.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/delegating_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/texture_uploader.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/quad_culler.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace {

void DidVisibilityChange(cc::LayerTreeHostImpl* id, bool visible) {
  if (visible) {
    TRACE_EVENT_ASYNC_BEGIN1("webkit",
                             "LayerTreeHostImpl::SetVisible",
                             id,
                             "LayerTreeHostImpl",
                             id);
    return;
  }

  TRACE_EVENT_ASYNC_END0("webkit", "LayerTreeHostImpl::SetVisible", id);
}

std::string ValueToString(scoped_ptr<base::Value> value) {
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
}

}  // namespace

namespace cc {

class LayerTreeHostImplTimeSourceAdapter : public TimeSourceClient {
 public:
  static scoped_ptr<LayerTreeHostImplTimeSourceAdapter> Create(
      LayerTreeHostImpl* layer_tree_host_impl,
      scoped_refptr<DelayBasedTimeSource> time_source) {
    return make_scoped_ptr(
        new LayerTreeHostImplTimeSourceAdapter(layer_tree_host_impl,
                                               time_source));
  }
  virtual ~LayerTreeHostImplTimeSourceAdapter() {
    time_source_->SetClient(NULL);
    time_source_->SetActive(false);
  }

  virtual void OnTimerTick() OVERRIDE {
    // In single threaded mode we attempt to simulate changing the current
    // thread by maintaining a fake thread id. When we switch from one
    // thread to another, we construct DebugScopedSetXXXThread objects that
    // update the thread id. This lets DCHECKS that ensure we're on the
    // right thread to work correctly in single threaded mode. The problem
    // here is that the timer tasks are run via the message loop, and when
    // they run, we've had no chance to construct a DebugScopedSetXXXThread
    // object. The result is that we report that we're running on the main
    // thread. In multi-threaded mode, this timer is run on the compositor
    // thread, so to keep this consistent in single-threaded mode, we'll
    // construct a DebugScopedSetImplThread object. There is no need to do
    // this in multi-threaded mode since the real thread id's will be
    // correct. In fact, setting fake thread id's interferes with the real
    // thread id's and causes breakage.
    scoped_ptr<DebugScopedSetImplThread> set_impl_thread;
    if (!layer_tree_host_impl_->proxy()->HasImplThread()) {
      set_impl_thread.reset(
          new DebugScopedSetImplThread(layer_tree_host_impl_->proxy()));
    }

    layer_tree_host_impl_->ActivatePendingTreeIfNeeded();
    layer_tree_host_impl_->Animate(base::TimeTicks::Now(), base::Time::Now());
    layer_tree_host_impl_->BeginNextFrame();
  }

  void SetActive(bool active) {
    if (active != time_source_->Active())
      time_source_->SetActive(active);
  }

 private:
  LayerTreeHostImplTimeSourceAdapter(
      LayerTreeHostImpl* layer_tree_host_impl,
      scoped_refptr<DelayBasedTimeSource> time_source)
      : layer_tree_host_impl_(layer_tree_host_impl),
        time_source_(time_source) {
    time_source_->SetClient(this);
  }

  LayerTreeHostImpl* layer_tree_host_impl_;
  scoped_refptr<DelayBasedTimeSource> time_source_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImplTimeSourceAdapter);
};

LayerTreeHostImpl::FrameData::FrameData()
    : contains_incomplete_tile(false) {}

LayerTreeHostImpl::FrameData::~FrameData() {}

scoped_ptr<LayerTreeHostImpl> LayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    Proxy* proxy,
    RenderingStatsInstrumentation* rendering_stats_instrumentation) {
  return make_scoped_ptr(
      new LayerTreeHostImpl(settings,
                            client,
                            proxy,
                            rendering_stats_instrumentation));
}

LayerTreeHostImpl::LayerTreeHostImpl(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    Proxy* proxy,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : client_(client),
      proxy_(proxy),
      did_lock_scrolling_layer_(false),
      should_bubble_scrolls_(false),
      wheel_scrolling_(false),
      settings_(settings),
      overdraw_bottom_height_(0.f),
      device_scale_factor_(1.f),
      visible_(true),
      managed_memory_policy_(
          PrioritizedResourceManager::DefaultMemoryAllocationLimit(),
          ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING,
          0,
          ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING),
      pinch_gesture_active_(false),
      fps_counter_(FrameRateCounter::Create(proxy_->HasImplThread())),
      paint_time_counter_(PaintTimeCounter::Create()),
      memory_history_(MemoryHistory::Create()),
      debug_rect_history_(DebugRectHistory::Create()),
      last_sent_memory_visible_bytes_(0),
      last_sent_memory_visible_and_nearby_bytes_(0),
      last_sent_memory_use_bytes_(0),
      animation_registrar_(AnimationRegistrar::Create()),
      rendering_stats_instrumentation_(rendering_stats_instrumentation) {
  DCHECK(proxy_->IsImplThread());
  DidVisibilityChange(this, visible_);

  SetDebugState(settings.initial_debug_state);

  if (settings.calculate_top_controls_position) {
    top_controls_manager_ =
        TopControlsManager::Create(this,
                                   settings.top_controls_height,
                                   settings.top_controls_show_threshold,
                                   settings.top_controls_hide_threshold);
  }

  SetDebugState(settings.initial_debug_state);

  // LTHI always has an active tree.
  active_tree_ = LayerTreeImpl::create(this);
}

LayerTreeHostImpl::~LayerTreeHostImpl() {
  DCHECK(proxy_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHostImpl::~LayerTreeHostImpl()");

  if (active_tree_->root_layer()) {
    ClearRenderSurfaces();
    // The layer trees must be destroyed before the layer tree host. We've
    // made a contract with our animation controllers that the registrar
    // will outlive them, and we must make good.
    recycle_tree_.reset();
    pending_tree_.reset();
    active_tree_.reset();
  }
}

void LayerTreeHostImpl::BeginCommit() {}

void LayerTreeHostImpl::CommitComplete() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::CommitComplete");

  // Impl-side painting needs an update immediately post-commit to have the
  // opportunity to create tilings.  Other paths can call UpdateDrawProperties
  // more lazily when needed prior to drawing.
  if (settings_.impl_side_painting) {
    pending_tree_->set_needs_update_draw_properties();
    pending_tree_->UpdateDrawProperties(LayerTreeImpl::UPDATE_PENDING_TREE);
  } else {
    active_tree_->set_needs_update_draw_properties();
  }

  client_->SendManagedMemoryStats();
}

bool LayerTreeHostImpl::CanDraw() {
  // Note: If you are changing this function or any other function that might
  // affect the result of CanDraw, make sure to call
  // client_->OnCanDrawStateChanged in the proper places and update the
  // NotifyIfCanDrawChanged test.

  if (!active_tree_->root_layer()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no root layer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (device_viewport_size_.IsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw empty viewport",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (active_tree_->ViewportSizeInvalid()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw viewport size recently changed",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (!renderer_) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no renderer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (active_tree_->ContentsTexturesPurged()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw contents textures purged",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return true;
}

void LayerTreeHostImpl::Animate(base::TimeTicks monotonic_time,
                                base::Time wall_clock_time) {
  AnimatePageScale(monotonic_time);
  AnimateLayers(monotonic_time, wall_clock_time);
  AnimateScrollbars(monotonic_time);
  AnimateTopControls(monotonic_time);
}

void LayerTreeHostImpl::ManageTiles() {
  DCHECK(tile_manager_);
  tile_manager_->ManageTiles();

  size_t memory_required_bytes;
  size_t memory_nice_to_have_bytes;
  size_t memory_used_bytes;
  tile_manager_->GetMemoryStats(&memory_required_bytes,
                                &memory_nice_to_have_bytes,
                                &memory_used_bytes);
  SendManagedMemoryStats(memory_required_bytes,
                         memory_nice_to_have_bytes,
                         memory_used_bytes);
}

void LayerTreeHostImpl::SetAnticipatedDrawTime(base::TimeTicks time) {
  if (tile_manager_)
    tile_manager_->SetAnticipatedDrawTime(time);
}

void LayerTreeHostImpl::StartPageScaleAnimation(gfx::Vector2d target_offset,
                                                bool anchor_point,
                                                float page_scale,
                                                base::TimeTicks start_time,
                                                base::TimeDelta duration) {
  if (!RootScrollLayer())
    return;

  gfx::Vector2dF scroll_total =
      RootScrollLayer()->scroll_offset() + RootScrollLayer()->scroll_delta();
  gfx::SizeF scaled_scrollable_size = active_tree_->ScrollableSize();
  gfx::SizeF viewport_size =
      gfx::ScaleSize(device_viewport_size_, 1.f / device_scale_factor_);

  double start_time_seconds = (start_time - base::TimeTicks()).InSecondsF();
  page_scale_animation_ =
      PageScaleAnimation::Create(scroll_total,
                                 active_tree_->total_page_scale_factor(),
                                 viewport_size,
                                 scaled_scrollable_size,
                                 start_time_seconds);

  if (anchor_point) {
    gfx::Vector2dF anchor(target_offset);
    page_scale_animation_->ZoomWithAnchor(anchor,
                                          page_scale,
                                          duration.InSecondsF());
  } else {
    gfx::Vector2dF scaled_target_offset = target_offset;
    page_scale_animation_->ZoomTo(scaled_target_offset,
                                  page_scale,
                                  duration.InSecondsF());
  }

  client_->SetNeedsRedrawOnImplThread();
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::ScheduleAnimation() {
  client_->SetNeedsRedrawOnImplThread();
}

bool LayerTreeHostImpl::HaveTouchEventHandlersAt(gfx::Point viewport_point) {
  if (!EnsureRenderSurfaceLayerList())
    return false;

  gfx::PointF device_viewport_point =
      gfx::ScalePoint(viewport_point, device_scale_factor_);

  // First find out which layer was hit from the saved list of visible layers
  // in the most recent frame.
  LayerImpl* layer_impl = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      device_viewport_point,
      active_tree_->RenderSurfaceLayerList());

  // Walk up the hierarchy and look for a layer with a touch event handler
  // region that the given point hits.
  for (; layer_impl; layer_impl = layer_impl->parent()) {
    if (LayerTreeHostCommon::LayerHasTouchEventHandlersAt(device_viewport_point,
                                                          layer_impl))
      return true;
  }

  return false;
}

void LayerTreeHostImpl::TrackDamageForAllSurfaces(
    LayerImpl* root_draw_layer,
    const LayerList& render_surface_layer_list) {
  // For now, we use damage tracking to compute a global scissor. To do this, we
  // must compute all damage tracking before drawing anything, so that we know
  // the root damage rect. The root damage rect is then used to scissor each
  // surface.

  for (int surface_index = render_surface_layer_list.size() - 1;
       surface_index >= 0 ;
       --surface_index) {
    LayerImpl* render_surface_layer = render_surface_layer_list[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();
    DCHECK(render_surface);
    render_surface->damage_tracker()->UpdateDamageTrackingState(
        render_surface->layer_list(),
        render_surface_layer->id(),
        render_surface->SurfacePropertyChangedOnlyFromDescendant(),
        render_surface->content_rect(),
        render_surface_layer->mask_layer(),
        render_surface_layer->filters(),
        render_surface_layer->filter().get());
  }
}

void LayerTreeHostImpl::FrameData::AppendRenderPass(
    scoped_ptr<RenderPass> render_pass) {
  render_passes_by_id[render_pass->id] = render_pass.get();
  render_passes.push_back(render_pass.Pass());
}

static void AppendQuadsForLayer(RenderPass* target_render_pass,
                                LayerImpl* layer,
                                const OcclusionTrackerImpl& occlusion_tracker,
                                AppendQuadsData* append_quads_data) {
  bool for_surface = false;
  QuadCuller quad_culler(&target_render_pass->quad_list,
                         &target_render_pass->shared_quad_state_list,
                         layer,
                         occlusion_tracker,
                         layer->ShowDebugBorders(),
                         for_surface);
  layer->AppendQuads(&quad_culler, append_quads_data);
}

static void AppendQuadsForRenderSurfaceLayer(
    RenderPass* target_render_pass,
    LayerImpl* layer,
    const RenderPass* contributing_render_pass,
    const OcclusionTrackerImpl& occlusion_tracker,
    AppendQuadsData* append_quads_data) {
  bool for_surface = true;
  QuadCuller quad_culler(&target_render_pass->quad_list,
                         &target_render_pass->shared_quad_state_list,
                         layer,
                         occlusion_tracker,
                         layer->ShowDebugBorders(),
                         for_surface);

  bool is_replica = false;
  layer->render_surface()->AppendQuads(&quad_culler,
                                       append_quads_data,
                                       is_replica,
                                       contributing_render_pass->id);

  // Add replica after the surface so that it appears below the surface.
  if (layer->has_replica()) {
    is_replica = true;
    layer->render_surface()->AppendQuads(&quad_culler,
                                         append_quads_data,
                                         is_replica,
                                         contributing_render_pass->id);
  }
}

static void AppendQuadsToFillScreen(
    RenderPass* target_render_pass,
    LayerImpl* root_layer,
    SkColor screen_background_color,
    const OcclusionTrackerImpl& occlusion_tracker) {
  if (!root_layer || !SkColorGetA(screen_background_color))
    return;

  Region fill_region = occlusion_tracker.ComputeVisibleRegionInScreen();
  if (fill_region.IsEmpty())
    return;

  bool for_surface = false;
  QuadCuller quad_culler(&target_render_pass->quad_list,
                         &target_render_pass->shared_quad_state_list,
                         root_layer,
                         occlusion_tracker,
                         root_layer->ShowDebugBorders(),
                         for_surface);

  // Manually create the quad state for the gutter quads, as the root layer
  // doesn't have any bounds and so can't generate this itself.
  // TODO(danakj): Make the gutter quads generated by the solid color layer
  // (make it smarter about generating quads to fill unoccluded areas).

  gfx::Rect root_target_rect = root_layer->render_surface()->content_rect();
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      quad_culler.UseSharedQuadState(SharedQuadState::Create());
  shared_quad_state->SetAll(root_layer->draw_transform(),
                            root_target_rect.size(),
                            root_target_rect,
                            root_target_rect,
                            false,
                            opacity);

  AppendQuadsData append_quads_data;

  gfx::Transform transform_to_layer_space(gfx::Transform::kSkipInitialization);
  bool did_invert = root_layer->screen_space_transform().GetInverse(
      &transform_to_layer_space);
  DCHECK(did_invert);
  for (Region::Iterator fill_rects(fill_region);
       fill_rects.has_rect();
       fill_rects.next()) {
    // The root layer transform is composed of translations and scales only,
    // no perspective, so mapping is sufficient (as opposed to projecting).
    gfx::Rect layer_rect =
        MathUtil::MapClippedRect(transform_to_layer_space, fill_rects.rect());
    // Skip the quad culler and just append the quads directly to avoid
    // occlusion checks.
    scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
    quad->SetNew(shared_quad_state, layer_rect, screen_background_color);
    quad_culler.Append(quad.PassAs<DrawQuad>(), &append_quads_data);
  }
}

bool LayerTreeHostImpl::CalculateRenderPasses(FrameData* frame) {
  DCHECK(frame->render_passes.empty());

  if (!CanDraw() || !active_tree_->root_layer())
    return false;

  TrackDamageForAllSurfaces(active_tree_->root_layer(),
                            *frame->render_surface_layer_list);

  TRACE_EVENT1("cc",
               "LayerTreeHostImpl::CalculateRenderPasses",
               "render_surface_layer_list.size()",
               static_cast<long long unsigned>(
                   frame->render_surface_layer_list->size()));

  // Create the render passes in dependency order.
  for (int surface_index = frame->render_surface_layer_list->size() - 1;
       surface_index >= 0 ;
       --surface_index) {
    LayerImpl* render_surface_layer =
        (*frame->render_surface_layer_list)[surface_index];
    render_surface_layer->render_surface()->AppendRenderPasses(frame);
  }

  bool record_metrics_for_frame =
      settings_.show_overdraw_in_tracing &&
      base::debug::TraceLog::GetInstance() &&
      base::debug::TraceLog::GetInstance()->IsEnabled();
  OcclusionTrackerImpl occlusion_tracker(
      active_tree_->root_layer()->render_surface()->content_rect(),
      record_metrics_for_frame);
  occlusion_tracker.set_minimum_tracking_size(
      settings_.minimum_occlusion_tracking_size);

  if (debug_state_.show_occluding_rects) {
    occlusion_tracker.set_occluding_screen_space_rects_container(
        &frame->occluding_screen_space_rects);
  }
  if (debug_state_.show_non_occluding_rects) {
    occlusion_tracker.set_non_occluding_screen_space_rects_container(
        &frame->non_occluding_screen_space_rects);
  }

  // Add quads to the Render passes in FrontToBack order to allow for testing
  // occlusion and performing culling during the tree walk.
  typedef LayerIterator<LayerImpl,
                        std::vector<LayerImpl*>,
                        RenderSurfaceImpl,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;

  // Typically when we are missing a texture and use a checkerboard quad, we
  // still draw the frame. However when the layer being checkerboarded is moving
  // due to an impl-animation, we drop the frame to avoid flashing due to the
  // texture suddenly appearing in the future.
  bool draw_frame = true;

  int layers_drawn = 0;

  LayerIteratorType end =
      LayerIteratorType::End(frame->render_surface_layer_list);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(frame->render_surface_layer_list);
       it != end;
       ++it) {
    RenderPass::Id target_render_pass_id =
        it.target_render_surface_layer()->render_surface()->RenderPassId();
    RenderPass* target_render_pass =
        frame->render_passes_by_id[target_render_pass_id];

    occlusion_tracker.EnterLayer(it);

    AppendQuadsData append_quads_data(target_render_pass->id);

    if (it.represents_contributing_render_surface()) {
      RenderPass::Id contributing_render_pass_id =
          it->render_surface()->RenderPassId();
      RenderPass* contributing_render_pass =
          frame->render_passes_by_id[contributing_render_pass_id];
      AppendQuadsForRenderSurfaceLayer(target_render_pass,
                                       *it,
                                       contributing_render_pass,
                                       occlusion_tracker,
                                       &append_quads_data);
    } else if (it.represents_itself() &&
               !it->visible_content_rect().IsEmpty()) {
      bool has_occlusion_from_outside_target_surface;
      bool impl_draw_transform_is_unknown = false;
      if (occlusion_tracker.Occluded(
              it->render_target(),
              it->visible_content_rect(),
              it->draw_transform(),
              impl_draw_transform_is_unknown,
              it->is_clipped(),
              it->clip_rect(),
              &has_occlusion_from_outside_target_surface)) {
        append_quads_data.had_occlusion_from_outside_target_surface |=
            has_occlusion_from_outside_target_surface;
      } else {
        DCHECK_EQ(active_tree_, it->layer_tree_impl());
        it->WillDraw(resource_provider_.get());
        frame->will_draw_layers.push_back(*it);

        if (it->HasContributingDelegatedRenderPasses()) {
          RenderPass::Id contributing_render_pass_id =
              it->FirstContributingRenderPassId();
          while (frame->render_passes_by_id.find(contributing_render_pass_id) !=
                 frame->render_passes_by_id.end()) {
            RenderPass* render_pass =
                frame->render_passes_by_id[contributing_render_pass_id];

            AppendQuadsData append_quads_data(render_pass->id);
            AppendQuadsForLayer(render_pass,
                                *it,
                                occlusion_tracker,
                                &append_quads_data);

            contributing_render_pass_id =
                it->NextContributingRenderPassId(contributing_render_pass_id);
          }
        }

        AppendQuadsForLayer(target_render_pass,
                            *it,
                            occlusion_tracker,
                            &append_quads_data);
      }

      ++layers_drawn;
    }

    if (append_quads_data.had_occlusion_from_outside_target_surface)
      target_render_pass->has_occlusion_from_outside_target_surface = true;

    if (append_quads_data.num_missing_tiles) {
      rendering_stats_instrumentation_->AddMissingTiles(
          append_quads_data.num_missing_tiles);
      bool layer_has_animating_transform =
          it->screen_space_transform_is_animating() ||
          it->draw_transform_is_animating();
      if (layer_has_animating_transform)
        draw_frame = false;
    }

    if (append_quads_data.had_incomplete_tile)
      frame->contains_incomplete_tile = true;

    occlusion_tracker.LeaveLayer(it);
  }

  rendering_stats_instrumentation_->AddLayersDrawn(layers_drawn);

#ifndef NDEBUG
  for (size_t i = 0; i < frame->render_passes.size(); ++i) {
    for (size_t j = 0; j < frame->render_passes[i]->quad_list.size(); ++j)
      DCHECK(frame->render_passes[i]->quad_list[j]->shared_quad_state);
    DCHECK(frame->render_passes_by_id.find(frame->render_passes[i]->id)
           != frame->render_passes_by_id.end());
  }
#endif
  DCHECK(frame->render_passes.back()->output_rect.origin().IsOrigin());

  if (!active_tree_->has_transparent_background()) {
    frame->render_passes.back()->has_transparent_background = false;
    AppendQuadsToFillScreen(frame->render_passes.back(),
                            active_tree_->root_layer(),
                            active_tree_->background_color(),
                            occlusion_tracker);
  }

  if (draw_frame)
    occlusion_tracker.overdraw_metrics()->RecordMetrics(this);

  RemoveRenderPasses(CullRenderPassesWithNoQuads(), frame);
  renderer_->DecideRenderPassAllocationsForFrame(frame->render_passes);
  RemoveRenderPasses(CullRenderPassesWithCachedTextures(*renderer_), frame);

  return draw_frame;
}

void LayerTreeHostImpl::SetBackgroundTickingEnabled(bool enabled) {
  // Lazily create the time_source adapter so that we can vary the interval for
  // testing.
  if (!time_source_client_adapter_) {
    time_source_client_adapter_ = LayerTreeHostImplTimeSourceAdapter::Create(
        this,
        DelayBasedTimeSource::Create(LowFrequencyAnimationInterval(),
                                     proxy_->CurrentThread()));
  }

  time_source_client_adapter_->SetActive(enabled);
}

static inline RenderPass* FindRenderPassById(
    RenderPass::Id render_pass_id,
    const LayerTreeHostImpl::FrameData& frame) {
  RenderPassIdHashMap::const_iterator it =
      frame.render_passes_by_id.find(render_pass_id);
  return it != frame.render_passes_by_id.end() ? it->second : NULL;
}

static void RemoveRenderPassesRecursive(RenderPass::Id remove_render_pass_id,
                                        LayerTreeHostImpl::FrameData* frame) {
  RenderPass* remove_render_pass =
      FindRenderPassById(remove_render_pass_id, *frame);
  // The pass was already removed by another quad - probably the original, and
  // we are the replica.
  if (!remove_render_pass)
    return;
  RenderPassList& render_passes = frame->render_passes;
  RenderPassList::iterator to_remove = std::find(render_passes.begin(),
                                                 render_passes.end(),
                                                 remove_render_pass);

  DCHECK(to_remove != render_passes.end());

  scoped_ptr<RenderPass> removed_pass = render_passes.take(to_remove);
  frame->render_passes.erase(to_remove);
  frame->render_passes_by_id.erase(remove_render_pass_id);

  // Now follow up for all RenderPass quads and remove their RenderPasses
  // recursively.
  const QuadList& quad_list = removed_pass->quad_list;
  QuadList::ConstBackToFrontIterator quad_list_iterator =
      quad_list.BackToFrontBegin();
  for (; quad_list_iterator != quad_list.BackToFrontEnd();
       ++quad_list_iterator) {
    DrawQuad* current_quad = (*quad_list_iterator);
    if (current_quad->material != DrawQuad::RENDER_PASS)
      continue;

    RenderPass::Id next_remove_render_pass_id =
        RenderPassDrawQuad::MaterialCast(current_quad)->render_pass_id;
    RemoveRenderPassesRecursive(next_remove_render_pass_id, frame);
  }
}

bool LayerTreeHostImpl::CullRenderPassesWithCachedTextures::
    ShouldRemoveRenderPass(const RenderPassDrawQuad& quad,
                           const FrameData& frame) const {
  bool quad_has_damage = !quad.contents_changed_since_last_frame.IsEmpty();
  bool quad_has_cached_resource =
      renderer_.HaveCachedResourcesForRenderPassId(quad.render_pass_id);
  if (quad_has_damage) {
    TRACE_EVENT0("cc", "CullRenderPassesWithCachedTextures have damage");
    return false;
  } else if (!quad_has_cached_resource) {
    TRACE_EVENT0("cc", "CullRenderPassesWithCachedTextures have no texture");
    return false;
  }
  TRACE_EVENT0("cc", "CullRenderPassesWithCachedTextures dropped!");
  return true;
}

bool LayerTreeHostImpl::CullRenderPassesWithNoQuads::ShouldRemoveRenderPass(
    const RenderPassDrawQuad& quad, const FrameData& frame) const {
  const RenderPass* render_pass =
      FindRenderPassById(quad.render_pass_id, frame);
  if (!render_pass)
    return false;

  // If any quad or RenderPass draws into this RenderPass, then keep it.
  const QuadList& quad_list = render_pass->quad_list;
  for (QuadList::ConstBackToFrontIterator quad_list_iterator =
           quad_list.BackToFrontBegin();
       quad_list_iterator != quad_list.BackToFrontEnd();
       ++quad_list_iterator) {
    DrawQuad* current_quad = *quad_list_iterator;

    if (current_quad->material != DrawQuad::RENDER_PASS)
      return false;

    const RenderPass* contributing_pass = FindRenderPassById(
        RenderPassDrawQuad::MaterialCast(current_quad)->render_pass_id, frame);
    if (contributing_pass)
      return false;
  }
  return true;
}

// Defined for linking tests.
template CC_EXPORT void LayerTreeHostImpl::RemoveRenderPasses<
  LayerTreeHostImpl::CullRenderPassesWithCachedTextures>(
      CullRenderPassesWithCachedTextures culler, FrameData* frame);
template CC_EXPORT void LayerTreeHostImpl::RemoveRenderPasses<
  LayerTreeHostImpl::CullRenderPassesWithNoQuads>(
      CullRenderPassesWithNoQuads culler, FrameData*);

// static
template <typename RenderPassCuller>
void LayerTreeHostImpl::RemoveRenderPasses(RenderPassCuller culler,
                                           FrameData* frame) {
  for (size_t it = culler.RenderPassListBegin(frame->render_passes);
       it != culler.RenderPassListEnd(frame->render_passes);
       it = culler.RenderPassListNext(it)) {
    const RenderPass* current_pass = frame->render_passes[it];
    const QuadList& quad_list = current_pass->quad_list;
    QuadList::ConstBackToFrontIterator quad_list_iterator =
        quad_list.BackToFrontBegin();

    for (; quad_list_iterator != quad_list.BackToFrontEnd();
         ++quad_list_iterator) {
      DrawQuad* current_quad = *quad_list_iterator;

      if (current_quad->material != DrawQuad::RENDER_PASS)
        continue;

      const RenderPassDrawQuad* render_pass_quad =
          RenderPassDrawQuad::MaterialCast(current_quad);
      if (!culler.ShouldRemoveRenderPass(*render_pass_quad, *frame))
        continue;

      // We are changing the vector in the middle of iteration. Because we
      // delete render passes that draw into the current pass, we are
      // guaranteed that any data from the iterator to the end will not
      // change. So, capture the iterator position from the end of the
      // list, and restore it after the change.
      size_t position_from_end = frame->render_passes.size() - it;
      RemoveRenderPassesRecursive(render_pass_quad->render_pass_id, frame);
      it = frame->render_passes.size() - position_from_end;
      DCHECK_GE(frame->render_passes.size(), position_from_end);
    }
  }
}

bool LayerTreeHostImpl::PrepareToDraw(FrameData* frame) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::PrepareToDraw");

  active_tree_->UpdateDrawProperties(
      LayerTreeImpl::UPDATE_ACTIVE_TREE_FOR_DRAW);

  frame->render_surface_layer_list = &active_tree_->RenderSurfaceLayerList();
  frame->render_passes.clear();
  frame->render_passes_by_id.clear();
  frame->will_draw_layers.clear();

  if (!CalculateRenderPasses(frame))
    return false;

  // If we return true, then we expect DrawLayers() to be called before this
  // function is called again.
  return true;
}

void LayerTreeHostImpl::EnforceManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  bool evicted_resources = client_->ReduceContentsTextureMemoryOnImplThread(
      visible_ ? policy.bytes_limit_when_visible
               : policy.bytes_limit_when_not_visible,
      ManagedMemoryPolicy::PriorityCutoffToValue(
          visible_ ? policy.priority_cutoff_when_visible
                   : policy.priority_cutoff_when_not_visible));
  if (evicted_resources) {
    active_tree_->SetContentsTexturesPurged();
    if (pending_tree_)
      pending_tree_->SetContentsTexturesPurged();
    client_->SetNeedsCommitOnImplThread();
    client_->OnCanDrawStateChanged(CanDraw());
    client_->RenewTreePriority();
  }
  client_->SendManagedMemoryStats();

  if (tile_manager_) {
    GlobalStateThatImpactsTilePriority new_state(tile_manager_->GlobalState());
    new_state.memory_limit_in_bytes = visible_ ?
                                      policy.bytes_limit_when_visible :
                                      policy.bytes_limit_when_not_visible;
    new_state.memory_limit_policy =
        ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
            visible_ ?
            policy.priority_cutoff_when_visible :
            policy.priority_cutoff_when_not_visible);
    tile_manager_->SetGlobalState(new_state);
  }
}

bool LayerTreeHostImpl::HasImplThread() const {
  return proxy_->HasImplThread();
}

void LayerTreeHostImpl::ScheduleManageTiles() {
  if (client_)
    client_->SetNeedsManageTilesOnImplThread();
}

void LayerTreeHostImpl::DidInitializeVisibleTile() {
  // TODO(reveman): Determine tiles that changed and only damage
  // what's necessary.
  SetFullRootLayerDamage();
  if (client_)
    client_->DidInitializeVisibleTileOnImplThread();
}

bool LayerTreeHostImpl::ShouldClearRootRenderPass() const {
  return settings_.should_clear_root_render_pass;
}

void LayerTreeHostImpl::SetManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (managed_memory_policy_ == policy)
    return;

  managed_memory_policy_ = policy;
  if (!proxy_->HasImplThread()) {
    // TODO(ccameron): In single-thread mode, this can be called on the main
    // thread by GLRenderer::OnMemoryAllocationChanged.
    DebugScopedSetImplThread impl_thread(proxy_);
    EnforceManagedMemoryPolicy(managed_memory_policy_);
  } else {
    DCHECK(proxy_->IsImplThread());
    EnforceManagedMemoryPolicy(managed_memory_policy_);
  }
  // We always need to commit after changing the memory policy because the new
  // limit can result in more or less content having texture allocated for it.
  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::OnVSyncParametersChanged(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  client_->OnVSyncParametersChanged(timebase, interval);
}

void LayerTreeHostImpl::OnSendFrameToParentCompositorAck(
    const CompositorFrameAck& ack) {
  if (!renderer_)
    return;

  // TODO(piman): We may need to do some validation on this ack before
  // processing it.
  renderer_->ReceiveCompositorFrameAck(ack);
}

void LayerTreeHostImpl::OnCanDrawStateChangedForTree() {
  client_->OnCanDrawStateChanged(CanDraw());
}

CompositorFrameMetadata LayerTreeHostImpl::MakeCompositorFrameMetadata() const {
  CompositorFrameMetadata metadata;
  metadata.device_scale_factor = device_scale_factor_;
  metadata.page_scale_factor = active_tree_->total_page_scale_factor();
  metadata.viewport_size = active_tree_->ScrollableViewportSize();
  metadata.root_layer_size = active_tree_->ScrollableSize();
  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  if (top_controls_manager_) {
    metadata.location_bar_offset =
        gfx::Vector2dF(0.f, top_controls_manager_->controls_top_offset());
    metadata.location_bar_content_translation =
        gfx::Vector2dF(0.f, top_controls_manager_->content_top_offset());
  }

  if (!RootScrollLayer())
    return metadata;

  metadata.root_scroll_offset = RootScrollLayer()->TotalScrollOffset();

  return metadata;
}

void LayerTreeHostImpl::DrawLayers(FrameData* frame,
                                   base::TimeTicks frame_begin_time) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::DrawLayers");
  DCHECK(CanDraw());
  DCHECK(!frame->render_passes.empty());

  fps_counter_->SaveTimeStamp(frame_begin_time);

  rendering_stats_instrumentation_->SetScreenFrameCount(
      fps_counter_->current_frame_number());
  rendering_stats_instrumentation_->SetDroppedFrameCount(
      fps_counter_->dropped_frame_count());

  if (tile_manager_) {
    memory_history_->SaveEntry(
        tile_manager_->memory_stats_from_last_assign());
  }

  if (debug_state_.ShowHudRects()) {
    debug_rect_history_->SaveDebugRectsForCurrentFrame(
        active_tree_->root_layer(),
        *frame->render_surface_layer_list,
        frame->occluding_screen_space_rects,
        frame->non_occluding_screen_space_rects,
        debug_state_);
  }

  if (debug_state_.trace_all_rendered_frames) {
    TRACE_EVENT_INSTANT1("cc.debug", "Frame", TRACE_EVENT_SCOPE_THREAD,
                         "frame", ValueToString(FrameStateAsValue()));
  }

  // Because the contents of the HUD depend on everything else in the frame, the
  // contents of its texture are updated as the last thing before the frame is
  // drawn.
  if (active_tree_->hud_layer())
    active_tree_->hud_layer()->UpdateHudTexture(resource_provider_.get());

  renderer_->DrawFrame(frame->render_passes);
  // The render passes should be consumed by the renderer.
  DCHECK(frame->render_passes.empty());
  frame->render_passes_by_id.clear();

  // The next frame should start by assuming nothing has changed, and changes
  // are noted as they occur.
  for (size_t i = 0; i < frame->render_surface_layer_list->size(); i++) {
    (*frame->render_surface_layer_list)[i]->render_surface()->damage_tracker()->
        DidDrawDamagedArea();
  }
  active_tree_->root_layer()->ResetAllChangeTrackingForSubtree();
  UpdateAnimationState();
}

void LayerTreeHostImpl::DidDrawAllLayers(const FrameData& frame) {
  for (size_t i = 0; i < frame.will_draw_layers.size(); ++i)
    frame.will_draw_layers[i]->DidDraw(resource_provider_.get());

  // Once all layers have been drawn, pending texture uploads should no
  // longer block future uploads.
  resource_provider_->MarkPendingUploadsAsNonBlocking();
}

void LayerTreeHostImpl::FinishAllRendering() {
  if (renderer_)
    renderer_->Finish();
}

bool LayerTreeHostImpl::IsContextLost() {
  DCHECK(proxy_->IsImplThread());
  return renderer_ && renderer_->IsContextLost();
}

const RendererCapabilities& LayerTreeHostImpl::GetRendererCapabilities() const {
  return renderer_->Capabilities();
}

bool LayerTreeHostImpl::SwapBuffers() {
  return renderer_->SwapBuffers();
}

gfx::Size LayerTreeHostImpl::DeviceViewportSize() const {
  return device_viewport_size();
}

gfx::SizeF LayerTreeHostImpl::VisibleViewportSize() const {
  gfx::SizeF dip_size =
      gfx::ScaleSize(DeviceViewportSize(), 1.f / device_scale_factor());

  // The clip layer should be used if non-overlay scrollbars may exist since
  // it adjusts for them.
  LayerImpl* clip_layer = active_tree_->RootClipLayer();
  if (!Settings().solid_color_scrollbars && clip_layer &&
      clip_layer->masks_to_bounds())
    dip_size = clip_layer->bounds();

  float top_offset =
      top_controls_manager_ ? top_controls_manager_->content_top_offset() : 0.f;
  return gfx::SizeF(dip_size.width(),
                    dip_size.height() - top_offset - overdraw_bottom_height_);
}

const LayerTreeSettings& LayerTreeHostImpl::Settings() const {
  return settings();
}

void LayerTreeHostImpl::DidLoseOutputSurface() {
  client_->DidLoseOutputSurfaceOnImplThread();
}

void LayerTreeHostImpl::OnSwapBuffersComplete() {
  client_->OnSwapBuffersCompleteOnImplThread();
}

void LayerTreeHostImpl::Readback(void* pixels,
                                 gfx::Rect rect_in_device_viewport) {
  DCHECK(renderer_);
  renderer_->GetFramebufferPixels(pixels, rect_in_device_viewport);
}

bool LayerTreeHostImpl::haveRootScrollLayer() const {
  return RootScrollLayer();
}

LayerImpl* LayerTreeHostImpl::RootLayer() const {
  return active_tree_->root_layer();
}

LayerImpl* LayerTreeHostImpl::RootScrollLayer() const {
  return active_tree_->RootScrollLayer();
}

LayerImpl* LayerTreeHostImpl::CurrentlyScrollingLayer() const {
  return active_tree_->CurrentlyScrollingLayer();
}

// Content layers can be either directly scrollable or contained in an outer
// scrolling layer which applies the scroll transform. Given a content layer,
// this function returns the associated scroll layer if any.
static LayerImpl* FindScrollLayerForContentLayer(LayerImpl* layer_impl) {
  if (!layer_impl)
    return 0;

  if (layer_impl->scrollable())
    return layer_impl;

  if (layer_impl->DrawsContent() &&
      layer_impl->parent() &&
      layer_impl->parent()->scrollable())
    return layer_impl->parent();

  return 0;
}

void LayerTreeHostImpl::CreatePendingTree() {
  CHECK(!pending_tree_);
  if (recycle_tree_)
    recycle_tree_.swap(pending_tree_);
  else
    pending_tree_ = LayerTreeImpl::create(this);
  client_->OnCanDrawStateChanged(CanDraw());
  client_->OnHasPendingTreeStateChanged(pending_tree_);
  TRACE_EVENT_ASYNC_BEGIN0("cc", "PendingTree", pending_tree_.get());
  TRACE_EVENT_ASYNC_STEP0("cc",
                          "PendingTree", pending_tree_.get(), "waiting");
}

void LayerTreeHostImpl::CheckForCompletedTileUploads() {
  DCHECK(!client_->IsInsideDraw()) <<
      "Checking for completed uploads within a draw may trigger "
      "spurious redraws.";
  if (tile_manager_)
    tile_manager_->CheckForCompletedTileUploads();
}

bool LayerTreeHostImpl::ActivatePendingTreeIfNeeded() {
  if (!pending_tree_)
    return false;

  CHECK(tile_manager_);

  pending_tree_->UpdateDrawProperties(LayerTreeImpl::UPDATE_PENDING_TREE);

  TRACE_EVENT_ASYNC_STEP1("cc",
                          "PendingTree", pending_tree_.get(), "activate",
                          "state", ValueToString(ActivationStateAsValue()));

  // It's always fine to activate to an empty tree.  Otherwise, only
  // activate once all visible resources in pending tree are ready
  // or tile manager has no work scheduled for pending tree.
  if (active_tree_->root_layer() &&
      !pending_tree_->AreVisibleResourcesReady()) {
    // In smoothness takes priority mode, the pending tree's priorities are
    // ignored, so the tile manager may not have work for it even though it
    // is simultaneously not ready to be activated.
    if (tile_manager_->GlobalState().tree_priority ==
        SMOOTHNESS_TAKES_PRIORITY ||
        tile_manager_->HasPendingWorkScheduled(PENDING_TREE)) {
      TRACE_EVENT_ASYNC_STEP0("cc",
                              "PendingTree",
                              pending_tree_.get(),
                              "waiting");
      return false;
    }
  }

  ActivatePendingTree();
  return true;
}

void LayerTreeHostImpl::ActivatePendingTree() {
  CHECK(pending_tree_);
  TRACE_EVENT_ASYNC_END0("cc", "PendingTree", pending_tree_.get());

  active_tree_->PushPersistedState(pending_tree_.get());
  if (pending_tree_->needs_full_tree_sync()) {
    active_tree_->SetRootLayer(
        TreeSynchronizer::SynchronizeTrees(pending_tree_->root_layer(),
                                           active_tree_->DetachLayerTree(),
                                           active_tree_.get()));
  }
  TreeSynchronizer::PushProperties(pending_tree_->root_layer(),
                                   active_tree_->root_layer());
  DCHECK(!recycle_tree_);

  pending_tree_->PushPropertiesTo(active_tree_.get());

  // Now that we've synced everything from the pending tree to the active
  // tree, rename the pending tree the recycle tree so we can reuse it on the
  // next sync.
  pending_tree_.swap(recycle_tree_);
  recycle_tree_->ClearRenderSurfaces();

  active_tree_->DidBecomeActive();

  // Reduce wasted memory now that unlinked resources are guaranteed not
  // to be used.
  client_->ReduceWastedContentsTextureMemoryOnImplThread();

  client_->OnCanDrawStateChanged(CanDraw());
  client_->OnHasPendingTreeStateChanged(pending_tree_);
  client_->SetNeedsRedrawOnImplThread();
  client_->RenewTreePriority();

  if (tile_manager_ && debug_state_.continuous_painting) {
    RenderingStats stats =
        rendering_stats_instrumentation_->GetRenderingStats();
    paint_time_counter_->SaveRasterizeTime(
        stats.total_rasterize_time_for_now_bins_on_pending_tree,
        active_tree_->source_frame_number());
  }
}

void LayerTreeHostImpl::SetVisible(bool visible) {
  DCHECK(proxy_->IsImplThread());

  if (visible_ == visible)
    return;
  visible_ = visible;
  DidVisibilityChange(this, visible_);
  EnforceManagedMemoryPolicy(managed_memory_policy_);

  if (!renderer_)
    return;

  renderer_->SetVisible(visible);

  SetBackgroundTickingEnabled(
      !visible_ &&
      !animation_registrar_->active_animation_controllers().empty());
}

bool LayerTreeHostImpl::InitializeRenderer(
    scoped_ptr<OutputSurface> output_surface) {
  // Since we will create a new resource provider, we cannot continue to use
  // the old resources (i.e. render_surfaces and texture IDs). Clear them
  // before we destroy the old resource provider.
  if (active_tree_->root_layer())
    ClearRenderSurfaces();
  if (active_tree_->root_layer())
    SendDidLoseOutputSurfaceRecursive(active_tree_->root_layer());
  if (pending_tree_ && pending_tree_->root_layer())
    SendDidLoseOutputSurfaceRecursive(pending_tree_->root_layer());
  if (recycle_tree_ && recycle_tree_->root_layer())
    SendDidLoseOutputSurfaceRecursive(recycle_tree_->root_layer());

  // Note: order is important here.
  renderer_.reset();
  tile_manager_.reset();
  resource_provider_.reset();
  output_surface_.reset();

  if (!output_surface->BindToClient(this))
    return false;

  scoped_ptr<ResourceProvider> resource_provider =
      ResourceProvider::Create(output_surface.get());
  if (!resource_provider)
    return false;

  if (settings_.impl_side_painting) {
    tile_manager_.reset(new TileManager(this,
                                        resource_provider.get(),
                                        settings_.num_raster_threads,
                                        settings_.use_cheapness_estimator,
                                        settings_.use_color_estimator,
                                        settings_.prediction_benchmarking,
                                        rendering_stats_instrumentation_));
  }

  if (output_surface->capabilities().has_parent_compositor) {
    renderer_ = DelegatingRenderer::Create(this, output_surface.get(),
                                           resource_provider.get());
  } else if (output_surface->context3d()) {
    renderer_ = GLRenderer::Create(this,
                                   output_surface.get(),
                                   resource_provider.get());
  } else if (output_surface->software_device()) {
    renderer_ = SoftwareRenderer::Create(this,
                                         output_surface.get(),
                                         resource_provider.get());
  }
  if (!renderer_)
    return false;

  resource_provider_ = resource_provider.Pass();
  output_surface_ = output_surface.Pass();

  if (!visible_)
    renderer_->SetVisible(visible_);

  client_->OnCanDrawStateChanged(CanDraw());

  // See note in LayerTreeImpl::UpdateDrawProperties.  Renderer needs
  // to be initialized to get max texture size.
  active_tree_->set_needs_update_draw_properties();
  if (pending_tree_)
    pending_tree_->set_needs_update_draw_properties();

  return true;
}

void LayerTreeHostImpl::SetViewportSize(gfx::Size layout_viewport_size,
                                        gfx::Size device_viewport_size) {
  if (layout_viewport_size == layout_viewport_size_ &&
      device_viewport_size == device_viewport_size_)
    return;

  if (pending_tree_ && device_viewport_size_ != device_viewport_size)
    active_tree_->SetViewportSizeInvalid();

  layout_viewport_size_ = layout_viewport_size;
  device_viewport_size_ = device_viewport_size;

  UpdateMaxScrollOffset();

  if (renderer_)
    renderer_->ViewportChanged();

  client_->OnCanDrawStateChanged(CanDraw());
}

static void AdjustScrollsForPageScaleChange(LayerImpl* layer_impl,
                                            float page_scale_change) {
  if (!layer_impl)
    return;

  if (layer_impl->scrollable()) {
    // We need to convert impl-side scroll deltas to page_scale space.
    gfx::Vector2dF scroll_delta = layer_impl->scroll_delta();
    scroll_delta.Scale(page_scale_change);
    layer_impl->SetScrollDelta(scroll_delta);
  }

  for (size_t i = 0; i < layer_impl->children().size(); ++i)
    AdjustScrollsForPageScaleChange(layer_impl->children()[i],
                                    page_scale_change);
}

void LayerTreeHostImpl::SetOverdrawBottomHeight(float overdraw_bottom_height) {
  if (overdraw_bottom_height == overdraw_bottom_height_)
    return;
  overdraw_bottom_height_ = overdraw_bottom_height;

  UpdateMaxScrollOffset();
}

void LayerTreeHostImpl::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  UpdateMaxScrollOffset();
}

void LayerTreeHostImpl::UpdateMaxScrollOffset() {
  active_tree_->UpdateMaxScrollOffset();
}

void LayerTreeHostImpl::setActiveTreeNeedsUpdateDrawProperties() {
  active_tree_->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::setNeedsRedraw() {
  client_->SetNeedsRedrawOnImplThread();
}

bool LayerTreeHostImpl::EnsureRenderSurfaceLayerList() {
  active_tree_->UpdateDrawProperties(LayerTreeImpl::UPDATE_ACTIVE_TREE);
  return active_tree_->RenderSurfaceLayerList().size();
}

InputHandlerClient::ScrollStatus LayerTreeHostImpl::ScrollBegin(
    gfx::Point viewport_point, InputHandlerClient::ScrollInputType type) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBegin");

  if (top_controls_manager_)
    top_controls_manager_->ScrollBegin();

  DCHECK(!CurrentlyScrollingLayer());
  ClearCurrentlyScrollingLayer();

  if (!EnsureRenderSurfaceLayerList())
    return ScrollIgnored;

  gfx::PointF device_viewport_point = gfx::ScalePoint(viewport_point,
                                                      device_scale_factor_);

  // First find out which layer was hit from the saved list of visible layers
  // in the most recent frame.
  LayerImpl* layer_impl = LayerTreeHostCommon::FindLayerThatIsHitByPoint(
      device_viewport_point, active_tree_->RenderSurfaceLayerList());

  // Walk up the hierarchy and look for a scrollable layer.
  LayerImpl* potentially_scrolling_layer_impl = 0;
  for (; layer_impl; layer_impl = layer_impl->parent()) {
    // The content layer can also block attempts to scroll outside the main
    // thread.
    ScrollStatus status = layer_impl->TryScroll(device_viewport_point, type);
    if (status == ScrollOnMainThread) {
      rendering_stats_instrumentation_->IncrementMainThreadScrolls();
      UMA_HISTOGRAM_BOOLEAN("TryScroll.SlowScroll", true);
      active_tree()->DidBeginScroll();
      return ScrollOnMainThread;
    }

    LayerImpl* scroll_layer_impl = FindScrollLayerForContentLayer(layer_impl);
    if (!scroll_layer_impl)
      continue;

    status = scroll_layer_impl->TryScroll(device_viewport_point, type);

    // If any layer wants to divert the scroll event to the main thread, abort.
    if (status == ScrollOnMainThread) {
      rendering_stats_instrumentation_->IncrementMainThreadScrolls();
      UMA_HISTOGRAM_BOOLEAN("TryScroll.SlowScroll", true);
      active_tree()->DidBeginScroll();
      return ScrollOnMainThread;
    }

    if (status == ScrollStarted && !potentially_scrolling_layer_impl)
      potentially_scrolling_layer_impl = scroll_layer_impl;
  }

  // When hiding top controls is enabled and the controls are hidden or
  // overlaying the content, force scrolls to be enabled on the root layer to
  // allow bringing the top controls back into view.
  if (!potentially_scrolling_layer_impl && top_controls_manager_ &&
      top_controls_manager_->content_top_offset() !=
      settings_.top_controls_height) {
    potentially_scrolling_layer_impl = RootScrollLayer();
  }

  if (potentially_scrolling_layer_impl) {
    active_tree_->SetCurrentlyScrollingLayer(
        potentially_scrolling_layer_impl);
    should_bubble_scrolls_ = (type != NonBubblingGesture);
    wheel_scrolling_ = (type == Wheel);
    rendering_stats_instrumentation_->IncrementImplThreadScrolls();
    client_->RenewTreePriority();
    UMA_HISTOGRAM_BOOLEAN("TryScroll.SlowScroll", false);
    active_tree()->DidBeginScroll();
    return ScrollStarted;
  }
  return ScrollIgnored;
}

gfx::Vector2dF LayerTreeHostImpl::ScrollLayerWithViewportSpaceDelta(
    LayerImpl* layer_impl,
    float scale_from_viewport_to_screen_space,
    gfx::PointF viewport_point,
    gfx::Vector2dF viewport_delta) {
  // Layers with non-invertible screen space transforms should not have passed
  // the scroll hit test in the first place.
  DCHECK(layer_impl->screen_space_transform().IsInvertible());
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert = layer_impl->screen_space_transform().GetInverse(
      &inverse_screen_space_transform);
  // TODO: With the advent of impl-side crolling for non-root layers, we may
  // need to explicitly handle uninvertible transforms here.
  DCHECK(did_invert);

  gfx::PointF screen_space_point =
      gfx::ScalePoint(viewport_point, scale_from_viewport_to_screen_space);

  gfx::Vector2dF screen_space_delta = viewport_delta;
  screen_space_delta.Scale(scale_from_viewport_to_screen_space);

  // First project the scroll start and end points to local layer space to find
  // the scroll delta in layer coordinates.
  bool start_clipped, end_clipped;
  gfx::PointF screen_space_end_point = screen_space_point + screen_space_delta;
  gfx::PointF local_start_point =
      MathUtil::ProjectPoint(inverse_screen_space_transform,
                             screen_space_point,
                             &start_clipped);
  gfx::PointF local_end_point =
      MathUtil::ProjectPoint(inverse_screen_space_transform,
                             screen_space_end_point,
                             &end_clipped);

  // In general scroll point coordinates should not get clipped.
  DCHECK(!start_clipped);
  DCHECK(!end_clipped);
  if (start_clipped || end_clipped)
    return gfx::Vector2dF();

  // local_start_point and local_end_point are in content space but we want to
  // move them to layer space for scrolling.
  float width_scale = 1.f / layer_impl->contents_scale_x();
  float height_scale = 1.f / layer_impl->contents_scale_y();
  local_start_point.Scale(width_scale, height_scale);
  local_end_point.Scale(width_scale, height_scale);

  // Apply the scroll delta.
  gfx::Vector2dF previous_delta = layer_impl->scroll_delta();
  layer_impl->ScrollBy(local_end_point - local_start_point);

  // Get the end point in the layer's content space so we can apply its
  // ScreenSpaceTransform.
  gfx::PointF actual_local_end_point = local_start_point +
                                       layer_impl->scroll_delta() -
                                       previous_delta;
  gfx::PointF actual_local_content_end_point =
      gfx::ScalePoint(actual_local_end_point,
                      1.f / width_scale,
                      1.f / height_scale);

  // Calculate the applied scroll delta in viewport space coordinates.
  gfx::PointF actual_screen_space_end_point =
      MathUtil::MapPoint(layer_impl->screen_space_transform(),
                         actual_local_content_end_point,
                         &end_clipped);
  DCHECK(!end_clipped);
  if (end_clipped)
    return gfx::Vector2dF();
  gfx::PointF actual_viewport_end_point =
      gfx::ScalePoint(actual_screen_space_end_point,
                      1.f / scale_from_viewport_to_screen_space);
  return actual_viewport_end_point - viewport_point;
}

static gfx::Vector2dF ScrollLayerWithLocalDelta(LayerImpl* layer_impl,
                                                gfx::Vector2dF local_delta) {
  gfx::Vector2dF previous_delta(layer_impl->scroll_delta());
  layer_impl->ScrollBy(local_delta);
  return layer_impl->scroll_delta() - previous_delta;
}

bool LayerTreeHostImpl::ScrollBy(gfx::Point viewport_point,
                                 gfx::Vector2dF scroll_delta) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBy");
  if (!CurrentlyScrollingLayer())
    return false;

  gfx::Vector2dF pending_delta = scroll_delta;
  bool did_scroll = false;
  bool consume_by_top_controls = top_controls_manager_ &&
      (CurrentlyScrollingLayer() == RootScrollLayer() || scroll_delta.y() < 0);

  for (LayerImpl* layer_impl = CurrentlyScrollingLayer();
       layer_impl;
       layer_impl = layer_impl->parent()) {
    if (!layer_impl->scrollable())
      continue;

    // Only allow bubble scrolling when the scroll is in the direction to make
    // the top controls visible.
    if (consume_by_top_controls && layer_impl == RootScrollLayer()) {
      pending_delta = top_controls_manager_->ScrollBy(pending_delta);
      UpdateMaxScrollOffset();
    }

    gfx::Vector2dF applied_delta;
    // Gesture events need to be transformed from viewport coordinates to local
    // layer coordinates so that the scrolling contents exactly follow the
    // user's finger. In contrast, wheel events represent a fixed amount of
    // scrolling so we can just apply them directly.
    if (!wheel_scrolling_) {
      float scale_from_viewport_to_screen_space = device_scale_factor_;
      applied_delta =
          ScrollLayerWithViewportSpaceDelta(layer_impl,
                                            scale_from_viewport_to_screen_space,
                                            viewport_point, pending_delta);
    } else {
      applied_delta = ScrollLayerWithLocalDelta(layer_impl, pending_delta);
    }

    // If the layer wasn't able to move, try the next one in the hierarchy.
    float move_threshold_squared = 0.1f * 0.1f;
    if (applied_delta.LengthSquared() < move_threshold_squared) {
      if (should_bubble_scrolls_ || !did_lock_scrolling_layer_)
        continue;
      else
        break;
    }
    did_scroll = true;
    did_lock_scrolling_layer_ = true;
    if (!should_bubble_scrolls_) {
      active_tree_->SetCurrentlyScrollingLayer(layer_impl);
      break;
    }

    // If the applied delta is within 45 degrees of the input delta, bail out to
    // make it easier to scroll just one layer in one direction without
    // affecting any of its parents.
    float angle_threshold = 45;
    if (MathUtil::SmallestAngleBetweenVectors(
            applied_delta, pending_delta) < angle_threshold) {
      pending_delta = gfx::Vector2d();
      break;
    }

    // Allow further movement only on an axis perpendicular to the direction in
    // which the layer moved.
    gfx::Vector2dF perpendicular_axis(-applied_delta.y(), applied_delta.x());
    pending_delta = MathUtil::ProjectVector(pending_delta, perpendicular_axis);

    if (gfx::ToFlooredVector2d(pending_delta).IsZero())
      break;
  }

  active_tree()->DidUpdateScroll();
  if (did_scroll) {
    client_->SetNeedsCommitOnImplThread();
    client_->SetNeedsRedrawOnImplThread();
    client_->RenewTreePriority();
  }
  return did_scroll;
}

// This implements scrolling by page as described here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms645601(v=vs.85).aspx#_win32_The_Mouse_Wheel
// for events with WHEEL_PAGESCROLL set.
bool LayerTreeHostImpl::ScrollVerticallyByPage(
    gfx::Point viewport_point,
    WebKit::WebScrollbar::ScrollDirection direction) {
  DCHECK(wheel_scrolling_);

  for (LayerImpl* layer_impl = CurrentlyScrollingLayer();
       layer_impl;
       layer_impl = layer_impl->parent()) {
    if (!layer_impl->scrollable())
      continue;

    if (!layer_impl->vertical_scrollbar_layer())
      continue;

    float height = layer_impl->vertical_scrollbar_layer()->bounds().height();

    // These magical values match WebKit and are designed to scroll nearly the
    // entire visible content height but leave a bit of overlap.
    float page = std::max(height * 0.875f, 1.f);
    if (direction == WebKit::WebScrollbar::ScrollBackward)
      page = -page;

    gfx::Vector2dF delta = gfx::Vector2dF(0.f, page);

    gfx::Vector2dF applied_delta = ScrollLayerWithLocalDelta(layer_impl, delta);

    if (!applied_delta.IsZero()) {
      active_tree()->DidUpdateScroll();
      client_->SetNeedsCommitOnImplThread();
      client_->SetNeedsRedrawOnImplThread();
      client_->RenewTreePriority();
      return true;
    }

    active_tree_->SetCurrentlyScrollingLayer(layer_impl);
  }

  return false;
}

void LayerTreeHostImpl::ClearCurrentlyScrollingLayer() {
  active_tree_->ClearCurrentlyScrollingLayer();
  did_lock_scrolling_layer_ = false;
}

void LayerTreeHostImpl::ScrollEnd() {
  if (top_controls_manager_)
    top_controls_manager_->ScrollEnd();
  ClearCurrentlyScrollingLayer();
  active_tree()->DidEndScroll();
  StartScrollbarAnimation(base::TimeTicks::Now());
}

void LayerTreeHostImpl::PinchGestureBegin() {
  pinch_gesture_active_ = true;
  previous_pinch_anchor_ = gfx::Point();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::PinchGestureUpdate(float magnify_delta,
                                           gfx::Point anchor) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::PinchGestureUpdate");

  if (!RootScrollLayer())
    return;

  // Keep the center-of-pinch anchor specified by (x, y) in a stable
  // position over the course of the magnify.
  float page_scale_delta = active_tree_->page_scale_delta();
  gfx::PointF previous_scale_anchor =
      gfx::ScalePoint(anchor, 1.f / page_scale_delta);
  active_tree_->SetPageScaleDelta(page_scale_delta * magnify_delta);
  page_scale_delta = active_tree_->page_scale_delta();
  gfx::PointF new_scale_anchor =
      gfx::ScalePoint(anchor, 1.f / page_scale_delta);
  gfx::Vector2dF move = previous_scale_anchor - new_scale_anchor;

  previous_pinch_anchor_ = anchor;

  move.Scale(1 / active_tree_->page_scale_factor());

  RootScrollLayer()->ScrollBy(move);

  client_->SetNeedsCommitOnImplThread();
  client_->SetNeedsRedrawOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::PinchGestureEnd() {
  pinch_gesture_active_ = false;
  client_->SetNeedsCommitOnImplThread();
}

static void CollectScrollDeltas(ScrollAndScaleSet* scroll_info,
                                LayerImpl* layer_impl) {
  if (!layer_impl)
    return;

  gfx::Vector2d scroll_delta =
      gfx::ToFlooredVector2d(layer_impl->scroll_delta());
  if (!scroll_delta.IsZero()) {
    LayerTreeHostCommon::ScrollUpdateInfo scroll;
    scroll.layer_id = layer_impl->id();
    scroll.scroll_delta = scroll_delta;
    scroll_info->scrolls.push_back(scroll);
    layer_impl->SetSentScrollDelta(scroll_delta);
  }

  for (size_t i = 0; i < layer_impl->children().size(); ++i)
    CollectScrollDeltas(scroll_info, layer_impl->children()[i]);
}

scoped_ptr<ScrollAndScaleSet> LayerTreeHostImpl::ProcessScrollDeltas() {
  scoped_ptr<ScrollAndScaleSet> scroll_info(new ScrollAndScaleSet());

  CollectScrollDeltas(scroll_info.get(), active_tree_->root_layer());
  scroll_info->page_scale_delta = active_tree_->page_scale_delta();
  active_tree_->set_sent_page_scale_delta(scroll_info->page_scale_delta);

  return scroll_info.Pass();
}

void LayerTreeHostImpl::SetFullRootLayerDamage() {
  if (active_tree_->root_layer()) {
    RenderSurfaceImpl* render_surface =
        active_tree_->root_layer()->render_surface();
    if (render_surface)
      render_surface->damage_tracker()->ForceFullDamageNextUpdate();
  }
}

void LayerTreeHostImpl::AnimatePageScale(base::TimeTicks time) {
  if (!page_scale_animation_ || !RootScrollLayer())
    return;

  double monotonic_time = (time - base::TimeTicks()).InSecondsF();
  gfx::Vector2dF scroll_total = RootScrollLayer()->scroll_offset() +
                                RootScrollLayer()->scroll_delta();

  active_tree_->SetPageScaleDelta(
      page_scale_animation_->PageScaleFactorAtTime(monotonic_time) /
      active_tree_->page_scale_factor());
  gfx::Vector2dF next_scroll =
      page_scale_animation_->ScrollOffsetAtTime(monotonic_time);

  RootScrollLayer()->ScrollBy(next_scroll - scroll_total);
  client_->SetNeedsRedrawOnImplThread();

  if (page_scale_animation_->IsAnimationCompleteAtTime(monotonic_time)) {
    page_scale_animation_.reset();
    client_->SetNeedsCommitOnImplThread();
    client_->RenewTreePriority();
  }
}

void LayerTreeHostImpl::AnimateTopControls(base::TimeTicks time) {
  if (!top_controls_manager_ || !RootScrollLayer())
    return;
  gfx::Vector2dF scroll = top_controls_manager_->Animate(time);
  UpdateMaxScrollOffset();
  RootScrollLayer()->ScrollBy(gfx::ScaleVector2d(
      scroll, 1.f / active_tree_->total_page_scale_factor()));
}

void LayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonic_time,
                                      base::Time wall_clock_time) {
  if (!settings_.accelerated_animation_enabled ||
      animation_registrar_->active_animation_controllers().empty() ||
      !active_tree_->root_layer())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::AnimateLayers");

  last_animation_time_ = wall_clock_time;
  double monotonic_seconds = (monotonic_time - base::TimeTicks()).InSecondsF();

  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter)
    (*iter).second->Animate(monotonic_seconds);

  client_->SetNeedsRedrawOnImplThread();
  SetBackgroundTickingEnabled(
      !visible_ &&
      !animation_registrar_->active_animation_controllers().empty());
}

void LayerTreeHostImpl::UpdateAnimationState() {
  if (!settings_.accelerated_animation_enabled ||
      animation_registrar_->active_animation_controllers().empty() ||
      !active_tree_->root_layer())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::UpdateAnimationState");
  scoped_ptr<AnimationEventsVector> events =
      make_scoped_ptr(new AnimationEventsVector);
  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter)
    (*iter).second->UpdateState(events.get());

  if (!events->empty()) {
    client_->PostAnimationEventsToMainThreadOnImplThread(events.Pass(),
                                                         last_animation_time_);
  }
}

base::TimeDelta LayerTreeHostImpl::LowFrequencyAnimationInterval() const {
  return base::TimeDelta::FromSeconds(1);
}

void LayerTreeHostImpl::SendDidLoseOutputSurfaceRecursive(LayerImpl* current) {
  DCHECK(current);
  current->DidLoseOutputSurface();
  if (current->mask_layer())
    SendDidLoseOutputSurfaceRecursive(current->mask_layer());
  if (current->replica_layer())
    SendDidLoseOutputSurfaceRecursive(current->replica_layer());
  for (size_t i = 0; i < current->children().size(); ++i)
    SendDidLoseOutputSurfaceRecursive(current->children()[i]);
}

void LayerTreeHostImpl::ClearRenderSurfaces() {
  active_tree_->ClearRenderSurfaces();
  if (pending_tree_)
    pending_tree_->ClearRenderSurfaces();
}

std::string LayerTreeHostImpl::LayerTreeAsText() const {
  std::string str;
  if (active_tree_->root_layer()) {
    str = active_tree_->root_layer()->LayerTreeAsText();
    str +=  "RenderSurfaces:\n";
    DumpRenderSurfaces(&str, 1, active_tree_->root_layer());
  }
  return str;
}

std::string LayerTreeHostImpl::LayerTreeAsJson() const {
  std::string str;
  if (active_tree_->root_layer()) {
    scoped_ptr<base::Value> json(active_tree_->root_layer()->LayerTreeAsJson());
    base::JSONWriter::WriteWithOptions(
        json.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &str);
  }
  return str;
}

void LayerTreeHostImpl::DumpRenderSurfaces(std::string* str,
                                           int indent,
                                           const LayerImpl* layer) const {
  if (layer->render_surface())
    layer->render_surface()->DumpSurface(str, indent);

  for (size_t i = 0; i < layer->children().size(); ++i)
    DumpRenderSurfaces(str, indent, layer->children()[i]);
}

int LayerTreeHostImpl::SourceAnimationFrameNumber() const {
  return fps_counter_->current_frame_number();
}

void LayerTreeHostImpl::SendManagedMemoryStats(
    size_t memory_visible_bytes,
    size_t memory_visible_and_nearby_bytes,
    size_t memory_use_bytes) {
  if (!renderer_)
    return;

  // Round the numbers being sent up to the next 8MB, to throttle the rate
  // at which we spam the GPU process.
  static const size_t rounding_step = 8 * 1024 * 1024;
  memory_visible_bytes = RoundUp(memory_visible_bytes, rounding_step);
  memory_visible_and_nearby_bytes = RoundUp(memory_visible_and_nearby_bytes,
                                            rounding_step);
  memory_use_bytes = RoundUp(memory_use_bytes, rounding_step);
  if (last_sent_memory_visible_bytes_ == memory_visible_bytes &&
      last_sent_memory_visible_and_nearby_bytes_ ==
          memory_visible_and_nearby_bytes &&
      last_sent_memory_use_bytes_ == memory_use_bytes) {
    return;
  }
  last_sent_memory_visible_bytes_ = memory_visible_bytes;
  last_sent_memory_visible_and_nearby_bytes_ = memory_visible_and_nearby_bytes;
  last_sent_memory_use_bytes_ = memory_use_bytes;

  renderer_->SendManagedMemoryStats(last_sent_memory_visible_bytes_,
                                    last_sent_memory_visible_and_nearby_bytes_,
                                    last_sent_memory_use_bytes_);
}

void LayerTreeHostImpl::AnimateScrollbars(base::TimeTicks time) {
  AnimateScrollbarsRecursive(active_tree_->root_layer(), time);
}

void LayerTreeHostImpl::AnimateScrollbarsRecursive(LayerImpl* layer,
                                                   base::TimeTicks time) {
  if (!layer)
    return;

  ScrollbarAnimationController* scrollbar_controller =
      layer->scrollbar_animation_controller();
  if (scrollbar_controller && scrollbar_controller->Animate(time)) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::SetNeedsRedraw due to AnimateScrollbars",
        TRACE_EVENT_SCOPE_THREAD);
    client_->SetNeedsRedrawOnImplThread();
  }

  for (size_t i = 0; i < layer->children().size(); ++i)
    AnimateScrollbarsRecursive(layer->children()[i], time);
}

void LayerTreeHostImpl::StartScrollbarAnimation(base::TimeTicks time) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::StartScrollbarAnimation");
  StartScrollbarAnimationRecursive(RootLayer(), time);
}

void LayerTreeHostImpl::StartScrollbarAnimationRecursive(LayerImpl* layer,
                                                         base::TimeTicks time) {
  if (!layer)
    return;

  ScrollbarAnimationController* scrollbar_controller =
      layer->scrollbar_animation_controller();
  if (scrollbar_controller && scrollbar_controller->IsAnimating()) {
    base::TimeDelta delay = scrollbar_controller->DelayBeforeStart(time);
    if (delay > base::TimeDelta())
      client_->RequestScrollbarAnimationOnImplThread(delay);
    else if (scrollbar_controller->Animate(time))
      client_->SetNeedsRedrawOnImplThread();
  }

  for (size_t i = 0; i < layer->children().size(); ++i)
    StartScrollbarAnimationRecursive(layer->children()[i], time);
}

void LayerTreeHostImpl::SetTreePriority(TreePriority priority) {
  if (!tile_manager_)
    return;

  GlobalStateThatImpactsTilePriority new_state(tile_manager_->GlobalState());
  if (new_state.tree_priority == priority)
    return;

  new_state.tree_priority = priority;
  tile_manager_->SetGlobalState(new_state);
}

void LayerTreeHostImpl::BeginNextFrame() {
  current_frame_time_ = base::TimeTicks();
}

base::TimeTicks LayerTreeHostImpl::CurrentFrameTime() {
  if (current_frame_time_.is_null())
    current_frame_time_ = base::TimeTicks::Now();
  return current_frame_time_;
}

scoped_ptr<base::Value> LayerTreeHostImpl::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->Set("activation_state", ActivationStateAsValue().release());
  state->Set("frame_state", FrameStateAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> LayerTreeHostImpl::ActivationStateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetString("lthi_id", base::StringPrintf("%p", this));
  state->SetBoolean("visible_resources_ready",
                    pending_tree_->AreVisibleResourcesReady());
  state->Set("tile_manager", tile_manager_->BasicStateAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> LayerTreeHostImpl::FrameStateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetString("lthi_id", base::StringPrintf("%p", this));
  state->Set("device_viewport_size",
             MathUtil::AsValue(device_viewport_size_).release());
  if (tile_manager_)
    state->Set("tiles", tile_manager_->AllTilesAsValue().release());
  state->Set("active_tree", active_tree_->AsValue().release());
  return state.PassAs<base::Value>();
}

// static
LayerImpl* LayerTreeHostImpl::GetNonCompositedContentLayerRecursive(
    LayerImpl* layer) {
  if (!layer)
    return NULL;

  if (layer->DrawsContent())
    return layer;

  for (LayerImpl::LayerList::const_iterator it = layer->children().begin();
       it != layer->children().end(); ++it) {
    LayerImpl* nccr = GetNonCompositedContentLayerRecursive(*it);
    if (nccr)
      return nccr;
  }

  return NULL;
}

skia::RefPtr<SkPicture> LayerTreeHostImpl::CapturePicture() {
  LayerTreeImpl* tree =
      pending_tree_ ? pending_tree_.get() : active_tree_.get();
  LayerImpl* layer = GetNonCompositedContentLayerRecursive(tree->root_layer());
  return layer ? layer->GetPicture() : skia::RefPtr<SkPicture>();
}

void LayerTreeHostImpl::SetDebugState(const LayerTreeDebugState& debug_state) {
  if (debug_state_.continuous_painting != debug_state.continuous_painting)
    paint_time_counter_->ClearHistory();

  debug_state_ = debug_state;
}

void LayerTreeHostImpl::SavePaintTime(const base::TimeDelta& total_paint_time,
                                      int commit_number) {
  DCHECK(debug_state_.continuous_painting);
  paint_time_counter_->SavePaintTime(total_paint_time, commit_number);
}

}  // namespace cc
