// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <limits>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/base/latency_info_swap_promise_monitor.h"
#include "cc/base/swap_promise.h"
#include "cc/base/switches.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "cc/trees/layer_tree_host.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebSelectionBound.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/gfx/frame_time.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "ui/gfx/android/device_display_info.h"
#endif

namespace base {
class Value;
}

namespace cc {
class Layer;
}

using blink::WebBeginFrameArgs;
using blink::WebFloatPoint;
using blink::WebRect;
using blink::WebSelectionBound;
using blink::WebSize;

namespace content {
namespace {

bool GetSwitchValueAsInt(
    const CommandLine& command_line,
    const std::string& switch_string,
    int min_value,
    int max_value,
    int* result) {
  std::string string_value = command_line.GetSwitchValueASCII(switch_string);
  int int_value;
  if (base::StringToInt(string_value, &int_value) &&
      int_value >= min_value && int_value <= max_value) {
    *result = int_value;
    return true;
  } else {
    LOG(WARNING) << "Failed to parse switch " << switch_string  << ": " <<
        string_value;
    return false;
  }
}

cc::LayerSelectionBound ConvertWebSelectionBound(
    const WebSelectionBound& web_bound) {
  DCHECK(web_bound.layerId);

  cc::LayerSelectionBound cc_bound;
  switch (web_bound.type) {
    case blink::WebSelectionBound::Caret:
      cc_bound.type = cc::SELECTION_BOUND_CENTER;
      break;
    case blink::WebSelectionBound::SelectionLeft:
      cc_bound.type = cc::SELECTION_BOUND_LEFT;
      break;
    case blink::WebSelectionBound::SelectionRight:
      cc_bound.type = cc::SELECTION_BOUND_RIGHT;
      break;
  }
  cc_bound.layer_id = web_bound.layerId;
  cc_bound.edge_top = gfx::Point(web_bound.edgeTopInLayer);
  cc_bound.edge_bottom = gfx::Point(web_bound.edgeBottomInLayer);
  return cc_bound;
}

gfx::Size CalculateDefaultTileSize() {
  int default_tile_size = 256;
#if defined(OS_ANDROID)
  // TODO(epenner): unify this for all platforms if it
  // makes sense (http://crbug.com/159524)

  gfx::DeviceDisplayInfo info;
  bool real_size_supported = true;
  int display_width = info.GetPhysicalDisplayWidth();
  int display_height = info.GetPhysicalDisplayHeight();
  if (display_width == 0 || display_height == 0) {
    real_size_supported = false;
    display_width = info.GetDisplayWidth();
    display_height = info.GetDisplayHeight();
  }

  int portrait_width = std::min(display_width, display_height);
  int landscape_width = std::max(display_width, display_height);

  if (real_size_supported) {
    // Maximum HD dimensions should be 768x1280
    // Maximum FHD dimensions should be 1200x1920
    if (portrait_width > 768 || landscape_width > 1280)
      default_tile_size = 384;
    if (portrait_width > 1200 || landscape_width > 1920)
      default_tile_size = 512;

    // Adjust for some resolutions that barely straddle an extra
    // tile when in portrait mode. This helps worst case scroll/raster
    // by not needing a full extra tile for each row.
    if (default_tile_size == 256 && portrait_width == 768)
      default_tile_size += 32;
    if (default_tile_size == 384 && portrait_width == 1200)
      default_tile_size += 32;
  } else {
    // We don't know the exact resolution due to screen controls etc.
    // So this just estimates the values above using tile counts.
    int numTiles = (display_width * display_height) / (256 * 256);
    if (numTiles > 16)
      default_tile_size = 384;
    if (numTiles >= 40)
      default_tile_size = 512;
  }
#endif
  return gfx::Size(default_tile_size, default_tile_size);
}

}  // namespace

// static
scoped_ptr<RenderWidgetCompositor> RenderWidgetCompositor::Create(
    RenderWidget* widget,
    bool threaded) {
  scoped_ptr<RenderWidgetCompositor> compositor(
      new RenderWidgetCompositor(widget, threaded));

  CommandLine* cmd = CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  settings.throttle_frame_production =
      !cmd->HasSwitch(switches::kDisableGpuVsync);
  settings.begin_frame_scheduling_enabled =
      cmd->HasSwitch(switches::kEnableBeginFrameScheduling);
  settings.main_frame_before_activation_enabled =
      cmd->HasSwitch(cc::switches::kEnableMainFrameBeforeActivation) &&
      !cmd->HasSwitch(cc::switches::kDisableMainFrameBeforeActivation);
  settings.main_frame_before_draw_enabled =
      !cmd->HasSwitch(cc::switches::kDisableMainFrameBeforeDraw);
  settings.report_overscroll_only_for_scrollable_axes = true;
  settings.accelerated_animation_enabled =
      !cmd->HasSwitch(cc::switches::kDisableThreadedAnimation);

  settings.default_tile_size = CalculateDefaultTileSize();
  if (cmd->HasSwitch(switches::kDefaultTileWidth)) {
    int tile_width = 0;
    GetSwitchValueAsInt(*cmd,
                        switches::kDefaultTileWidth,
                        1,
                        std::numeric_limits<int>::max(),
                        &tile_width);
    settings.default_tile_size.set_width(tile_width);
  }
  if (cmd->HasSwitch(switches::kDefaultTileHeight)) {
    int tile_height = 0;
    GetSwitchValueAsInt(*cmd,
                        switches::kDefaultTileHeight,
                        1,
                        std::numeric_limits<int>::max(),
                        &tile_height);
    settings.default_tile_size.set_height(tile_height);
  }

  int max_untiled_layer_width = settings.max_untiled_layer_size.width();
  if (cmd->HasSwitch(switches::kMaxUntiledLayerWidth)) {
    GetSwitchValueAsInt(*cmd, switches::kMaxUntiledLayerWidth, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_width);
  }
  int max_untiled_layer_height = settings.max_untiled_layer_size.height();
  if (cmd->HasSwitch(switches::kMaxUntiledLayerHeight)) {
    GetSwitchValueAsInt(*cmd, switches::kMaxUntiledLayerHeight, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_height);
  }

  settings.max_untiled_layer_size = gfx::Size(max_untiled_layer_width,
                                           max_untiled_layer_height);

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // render_thread may be NULL in tests.
  if (render_thread) {
    settings.impl_side_painting =
        render_thread->is_impl_side_painting_enabled();
    settings.gpu_rasterization_forced =
        render_thread->is_gpu_rasterization_forced();
    settings.gpu_rasterization_enabled =
        render_thread->is_gpu_rasterization_enabled();
    settings.can_use_lcd_text = render_thread->is_lcd_text_enabled();
    settings.use_distance_field_text =
        render_thread->is_distance_field_text_enabled();
    settings.use_zero_copy = render_thread->is_zero_copy_enabled();
    settings.use_one_copy = render_thread->is_one_copy_enabled();
  }

  if (cmd->HasSwitch(switches::kEnableBleedingEdgeRenderingFastPaths)) {
    settings.recording_mode = cc::LayerTreeSettings::RecordWithSkRecord;
  }

  settings.calculate_top_controls_position =
      cmd->HasSwitch(cc::switches::kEnableTopControlsPositionCalculation);
  if (cmd->HasSwitch(cc::switches::kTopControlsHeight)) {
    std::string controls_height_str =
        cmd->GetSwitchValueASCII(cc::switches::kTopControlsHeight);
    double controls_height;
    if (base::StringToDouble(controls_height_str, &controls_height) &&
        controls_height > 0)
      settings.top_controls_height = controls_height;
  }

  if (settings.calculate_top_controls_position &&
      settings.top_controls_height <= 0) {
    DCHECK(false)
        << "Top controls repositioning enabled without valid height set.";
    settings.calculate_top_controls_position = false;
  }

  if (cmd->HasSwitch(cc::switches::kTopControlsShowThreshold)) {
      std::string top_threshold_str =
          cmd->GetSwitchValueASCII(cc::switches::kTopControlsShowThreshold);
      double show_threshold;
      if (base::StringToDouble(top_threshold_str, &show_threshold) &&
          show_threshold >= 0.f && show_threshold <= 1.f)
        settings.top_controls_show_threshold = show_threshold;
  }

  if (cmd->HasSwitch(cc::switches::kTopControlsHideThreshold)) {
      std::string top_threshold_str =
          cmd->GetSwitchValueASCII(cc::switches::kTopControlsHideThreshold);
      double hide_threshold;
      if (base::StringToDouble(top_threshold_str, &hide_threshold) &&
          hide_threshold >= 0.f && hide_threshold <= 1.f)
        settings.top_controls_hide_threshold = hide_threshold;
  }

  settings.use_pinch_virtual_viewport =
      cmd->HasSwitch(cc::switches::kEnablePinchVirtualViewport);
  settings.allow_antialiasing &=
      !cmd->HasSwitch(cc::switches::kDisableCompositedAntialiasing);
  settings.single_thread_proxy_scheduler =
      !cmd->HasSwitch(switches::kDisableSingleThreadProxyScheduler);

  // These flags should be mirrored by UI versions in ui/compositor/.
  settings.initial_debug_state.show_debug_borders =
      cmd->HasSwitch(cc::switches::kShowCompositedLayerBorders);
  settings.initial_debug_state.show_fps_counter =
      cmd->HasSwitch(cc::switches::kShowFPSCounter);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      cmd->HasSwitch(cc::switches::kShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      cmd->HasSwitch(switches::kShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      cmd->HasSwitch(cc::switches::kShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      cmd->HasSwitch(cc::switches::kShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      cmd->HasSwitch(cc::switches::kShowScreenSpaceRects);
  settings.initial_debug_state.show_replica_screen_space_rects =
      cmd->HasSwitch(cc::switches::kShowReplicaScreenSpaceRects);
  settings.initial_debug_state.show_occluding_rects =
      cmd->HasSwitch(cc::switches::kShowOccludingRects);
  settings.initial_debug_state.show_non_occluding_rects =
      cmd->HasSwitch(cc::switches::kShowNonOccludingRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      cmd->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  if (cmd->HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
    const int kMinSlowDownScaleFactor = 0;
    const int kMaxSlowDownScaleFactor = INT_MAX;
    GetSwitchValueAsInt(
        *cmd,
        cc::switches::kSlowDownRasterScaleFactor,
        kMinSlowDownScaleFactor,
        kMaxSlowDownScaleFactor,
        &settings.initial_debug_state.slow_down_raster_scale_factor);
  }

  if (cmd->HasSwitch(cc::switches::kMaxTilesForInterestArea)) {
    int max_tiles_for_interest_area;
    if (GetSwitchValueAsInt(*cmd,
                            cc::switches::kMaxTilesForInterestArea,
                            1, std::numeric_limits<int>::max(),
                            &max_tiles_for_interest_area))
      settings.max_tiles_for_interest_area = max_tiles_for_interest_area;
  }

  if (cmd->HasSwitch(cc::switches::kMaxUnusedResourceMemoryUsagePercentage)) {
    int max_unused_resource_memory_percentage;
    if (GetSwitchValueAsInt(
            *cmd,
            cc::switches::kMaxUnusedResourceMemoryUsagePercentage,
            0, 100,
            &max_unused_resource_memory_percentage)) {
      settings.max_unused_resource_memory_percentage =
          max_unused_resource_memory_percentage;
    }
  }

  settings.strict_layer_property_change_checking =
      cmd->HasSwitch(cc::switches::kStrictLayerPropertyChangeChecking);

#if defined(OS_ANDROID)
  SynchronousCompositorFactory* synchronous_compositor_factory =
      SynchronousCompositorFactory::GetInstance();

  settings.using_synchronous_renderer_compositor =
      synchronous_compositor_factory;
  settings.record_full_layer =
      synchronous_compositor_factory &&
      synchronous_compositor_factory->RecordFullLayer();
  settings.report_overscroll_only_for_scrollable_axes =
      !synchronous_compositor_factory;
  settings.max_partial_texture_updates = 0;
  if (synchronous_compositor_factory) {
    // Android WebView uses system scrollbars, so make ours invisible.
    settings.scrollbar_animator = cc::LayerTreeSettings::NoAnimator;
    settings.solid_color_scrollbar_color = SK_ColorTRANSPARENT;
  } else {
    settings.scrollbar_animator = cc::LayerTreeSettings::LinearFade;
    settings.scrollbar_fade_delay_ms = 300;
    settings.scrollbar_fade_duration_ms = 300;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings.highp_threshold_min = 2048;
  // Android WebView handles root layer flings itself.
  settings.ignore_root_layer_flings =
      synchronous_compositor_factory;
  // Memory policy on Android WebView does not depend on whether device is
  // low end, so always use default policy.
  bool is_low_end_device =
      base::SysInfo::IsLowEndDevice() && !synchronous_compositor_factory;
  // RGBA_4444 textures are only enabled for low end devices
  // and are disabled for Android WebView as it doesn't support the format.
  settings.use_rgba_4444_textures = is_low_end_device;
  if (is_low_end_device) {
    // On low-end we want to be very carefull about killing other
    // apps. So initially we use 50% more memory to avoid flickering
    // or raster-on-demand.
    settings.max_memory_for_prepaint_percentage = 67;
  } else {
    // On other devices we have increased memory excessively to avoid
    // raster-on-demand already, so now we reserve 50% _only_ to avoid
    // raster-on-demand, and use 50% of the memory otherwise.
    settings.max_memory_for_prepaint_percentage = 50;
  }
  // Webview does not own the surface so should not clear it.
  settings.should_clear_root_render_pass =
      !synchronous_compositor_factory;

  // TODO(danakj): Only do this on low end devices.
  settings.create_low_res_tiling = true;

#elif !defined(OS_MACOSX)
  if (ui::IsOverlayScrollbarEnabled()) {
    settings.scrollbar_animator = cc::LayerTreeSettings::Thinning;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  } else if (cmd->HasSwitch(cc::switches::kEnablePinchVirtualViewport)) {
    // use_pinch_zoom_scrollbars is only true on desktop when non-overlay
    // scrollbars are in use.
    settings.use_pinch_zoom_scrollbars = true;
    settings.scrollbar_animator = cc::LayerTreeSettings::LinearFade;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings.scrollbar_fade_delay_ms = 500;
  settings.scrollbar_fade_duration_ms = 300;
#endif

  if (cmd->HasSwitch(switches::kEnableLowResTiling))
    settings.create_low_res_tiling = true;
  if (cmd->HasSwitch(switches::kDisableLowResTiling))
    settings.create_low_res_tiling = false;

  compositor->Initialize(settings);

  return compositor.Pass();
}

RenderWidgetCompositor::RenderWidgetCompositor(RenderWidget* widget,
                                               bool threaded)
    : threaded_(threaded),
      widget_(widget),
      send_v8_idle_notification_after_commit_(true) {
  CommandLine* cmd = CommandLine::ForCurrentProcess();

  if (cmd->HasSwitch(switches::kEnableV8IdleNotificationAfterCommit))
    send_v8_idle_notification_after_commit_ = true;
  if (cmd->HasSwitch(switches::kDisableV8IdleNotificationAfterCommit))
    send_v8_idle_notification_after_commit_ = false;
}

RenderWidgetCompositor::~RenderWidgetCompositor() {}

const base::WeakPtr<cc::InputHandler>&
RenderWidgetCompositor::GetInputHandler() {
  return layer_tree_host_->GetInputHandler();
}

bool RenderWidgetCompositor::BeginMainFrameRequested() const {
  return layer_tree_host_->BeginMainFrameRequested();
}

void RenderWidgetCompositor::SetNeedsDisplayOnAllLayers() {
  layer_tree_host_->SetNeedsDisplayOnAllLayers();
}

void RenderWidgetCompositor::SetRasterizeOnlyVisibleContent() {
  cc::LayerTreeDebugState current = layer_tree_host_->debug_state();
  current.rasterize_only_visible_content = true;
  layer_tree_host_->SetDebugState(current);
}

void RenderWidgetCompositor::UpdateTopControlsState(
    cc::TopControlsState constraints,
    cc::TopControlsState current,
    bool animate) {
  layer_tree_host_->UpdateTopControlsState(constraints,
                                           current,
                                           animate);
}

void RenderWidgetCompositor::SetTopControlsLayoutHeight(float height) {
  layer_tree_host_->SetTopControlsLayoutHeight(height);
}

void RenderWidgetCompositor::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

void RenderWidgetCompositor::SetNeedsForcedRedraw() {
  layer_tree_host_->SetNextCommitForcesRedraw();
  setNeedsAnimate();
}

scoped_ptr<cc::SwapPromiseMonitor>
RenderWidgetCompositor::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return scoped_ptr<cc::SwapPromiseMonitor>(
      new cc::LatencyInfoSwapPromiseMonitor(
          latency, layer_tree_host_.get(), NULL));
}

void RenderWidgetCompositor::QueueSwapPromise(
    scoped_ptr<cc::SwapPromise> swap_promise) {
  layer_tree_host_->QueueSwapPromise(swap_promise.Pass());
}

int RenderWidgetCompositor::GetLayerTreeId() const {
  return layer_tree_host_->id();
}

int RenderWidgetCompositor::GetSourceFrameNumber() const {
  return layer_tree_host_->source_frame_number();
}

void RenderWidgetCompositor::SetNeedsCommit() {
  layer_tree_host_->SetNeedsCommit();
}

void RenderWidgetCompositor::NotifyInputThrottledUntilCommit() {
  layer_tree_host_->NotifyInputThrottledUntilCommit();
}

const cc::Layer* RenderWidgetCompositor::GetRootLayer() const {
  return layer_tree_host_->root_layer();
}

int RenderWidgetCompositor::ScheduleMicroBenchmark(
    const std::string& name,
    scoped_ptr<base::Value> value,
    const base::Callback<void(scoped_ptr<base::Value>)>& callback) {
  return layer_tree_host_->ScheduleMicroBenchmark(name, value.Pass(), callback);
}

bool RenderWidgetCompositor::SendMessageToMicroBenchmark(
    int id,
    scoped_ptr<base::Value> value) {
  return layer_tree_host_->SendMessageToMicroBenchmark(id, value.Pass());
}

void RenderWidgetCompositor::Initialize(cc::LayerTreeSettings settings) {
  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_proxy;
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner(base::MessageLoopProxy::current());
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  cc::SharedBitmapManager* shared_bitmap_manager = NULL;
  // render_thread may be NULL in tests.
  if (render_thread) {
    compositor_message_loop_proxy =
        render_thread->compositor_message_loop_proxy();
    shared_bitmap_manager = render_thread->shared_bitmap_manager();
    main_thread_compositor_task_runner =
        render_thread->main_thread_compositor_task_runner();
  }
  if (compositor_message_loop_proxy.get()) {
    layer_tree_host_ =
        cc::LayerTreeHost::CreateThreaded(this,
                                          shared_bitmap_manager,
                                          settings,
                                          main_thread_compositor_task_runner,
                                          compositor_message_loop_proxy);
  } else {
    layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(
        this,
        this,
        shared_bitmap_manager,
        settings,
        main_thread_compositor_task_runner);
  }
  DCHECK(layer_tree_host_);
}

void RenderWidgetCompositor::setSurfaceReady() {
  // In tests without a RenderThreadImpl, don't set ready as this kicks
  // off creating output surfaces that the test can't create.
  if (RenderThreadImpl::current())
    layer_tree_host_->SetLayerTreeHostClientReady();
}

void RenderWidgetCompositor::setRootLayer(const blink::WebLayer& layer) {
  layer_tree_host_->SetRootLayer(
      static_cast<const cc_blink::WebLayerImpl*>(&layer)->layer());
}

void RenderWidgetCompositor::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::setViewportSize(
    const WebSize&,
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

void RenderWidgetCompositor::setViewportSize(
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

WebSize RenderWidgetCompositor::layoutViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

WebSize RenderWidgetCompositor::deviceViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

WebFloatPoint RenderWidgetCompositor::adjustEventPointForPinchZoom(
    const WebFloatPoint& point) const {
  return point;
}

void RenderWidgetCompositor::setDeviceScaleFactor(float device_scale) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale);
}

float RenderWidgetCompositor::deviceScaleFactor() const {
  return layer_tree_host_->device_scale_factor();
}

void RenderWidgetCompositor::setBackgroundColor(blink::WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void RenderWidgetCompositor::setHasTransparentBackground(bool transparent) {
  layer_tree_host_->set_has_transparent_background(transparent);
}

void RenderWidgetCompositor::setOverhangBitmap(const SkBitmap& bitmap) {
  layer_tree_host_->SetOverhangBitmap(bitmap);
}

void RenderWidgetCompositor::setVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void RenderWidgetCompositor::setPageScaleFactorAndLimits(
    float page_scale_factor, float minimum, float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void RenderWidgetCompositor::startPageScaleAnimation(
    const blink::WebPoint& destination,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
      duration_sec * base::Time::kMicrosecondsPerSecond);
  layer_tree_host_->StartPageScaleAnimation(
      gfx::Vector2d(destination.x, destination.y),
      use_anchor,
      new_page_scale,
      duration);
}

void RenderWidgetCompositor::heuristicsForGpuRasterizationUpdated(
    bool matches_heuristics) {
  layer_tree_host_->SetHasGpuRasterizationTrigger(matches_heuristics);
}

void RenderWidgetCompositor::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
}

bool RenderWidgetCompositor::commitRequested() const {
  return layer_tree_host_->CommitRequested();
}

void RenderWidgetCompositor::didStopFlinging() {
  layer_tree_host_->DidStopFlinging();
}

void RenderWidgetCompositor::registerForAnimations(blink::WebLayer* layer) {
  cc::Layer* cc_layer = static_cast<cc_blink::WebLayerImpl*>(layer)->layer();
  cc_layer->layer_animation_controller()->SetAnimationRegistrar(
      layer_tree_host_->animation_registrar());
}

void RenderWidgetCompositor::registerViewportLayers(
    const blink::WebLayer* pageScaleLayer,
    const blink::WebLayer* innerViewportScrollLayer,
    const blink::WebLayer* outerViewportScrollLayer) {
  layer_tree_host_->RegisterViewportLayers(
      static_cast<const cc_blink::WebLayerImpl*>(pageScaleLayer)->layer(),
      static_cast<const cc_blink::WebLayerImpl*>(innerViewportScrollLayer)
          ->layer(),
      // The outer viewport layer will only exist when using pinch virtual
      // viewports.
      outerViewportScrollLayer ? static_cast<const cc_blink::WebLayerImpl*>(
                                     outerViewportScrollLayer)->layer()
                               : NULL);
}

void RenderWidgetCompositor::clearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::registerSelection(
    const blink::WebSelectionBound& start,
    const blink::WebSelectionBound& end) {
  layer_tree_host_->RegisterSelection(ConvertWebSelectionBound(start),
                                      ConvertWebSelectionBound(end));
}

void RenderWidgetCompositor::clearSelection() {
  cc::LayerSelectionBound empty_selection;
  layer_tree_host_->RegisterSelection(empty_selection, empty_selection);
}

void CompositeAndReadbackAsyncCallback(
    blink::WebCompositeAndReadbackAsyncCallback* callback,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->HasBitmap()) {
    scoped_ptr<SkBitmap> result_bitmap = result->TakeBitmap();
    callback->didCompositeAndReadback(*result_bitmap);
  } else {
    callback->didCompositeAndReadback(SkBitmap());
  }
}

void RenderWidgetCompositor::compositeAndReadbackAsync(
    blink::WebCompositeAndReadbackAsyncCallback* callback) {
  DCHECK(layer_tree_host_->root_layer());
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&CompositeAndReadbackAsyncCallback, callback));
  layer_tree_host_->root_layer()->RequestCopyOfOutput(request.Pass());

  if (!threaded_ &&
      !layer_tree_host_->settings().single_thread_proxy_scheduler) {
    layer_tree_host_->Composite(gfx::FrameTime::Now());
  }
}

void RenderWidgetCompositor::finishAllRendering() {
  layer_tree_host_->FinishAllRendering();
}

void RenderWidgetCompositor::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

void RenderWidgetCompositor::setShowFPSCounter(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_fps_counter = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowPaintRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_paint_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowDebugBorders(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_debug_borders = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setContinuousPaintingEnabled(bool enabled) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.continuous_painting = enabled;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowScrollBottleneckRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.show_touch_event_handler_rects = show;
  debug_state.show_wheel_event_handler_rects = show;
  debug_state.show_non_fast_scrollable_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setTopControlsContentOffset(float offset) {
  layer_tree_host_->SetTopControlsContentOffset(offset);
}

void RenderWidgetCompositor::WillBeginMainFrame(int frame_id) {
  widget_->InstrumentWillBeginFrame(frame_id);
  widget_->willBeginCompositorFrame();
}

void RenderWidgetCompositor::DidBeginMainFrame() {
  widget_->InstrumentDidBeginFrame();
}

void RenderWidgetCompositor::BeginMainFrame(const cc::BeginFrameArgs& args) {
  begin_main_frame_time_ = args.frame_time;
  begin_main_frame_interval_ = args.interval;
  double frame_time_sec = (args.frame_time - base::TimeTicks()).InSecondsF();
  double deadline_sec = (args.deadline - base::TimeTicks()).InSecondsF();
  double interval_sec = args.interval.InSecondsF();
  WebBeginFrameArgs web_begin_frame_args =
      WebBeginFrameArgs(frame_time_sec, deadline_sec, interval_sec);
  widget_->webwidget()->beginFrame(web_begin_frame_args);
}

void RenderWidgetCompositor::Layout() {
  widget_->webwidget()->layout();
}

void RenderWidgetCompositor::ApplyViewportDeltas(
    const gfx::Vector2d& scroll_delta,
    float page_scale,
    float top_controls_delta) {
  widget_->webwidget()->applyViewportDeltas(
      scroll_delta,
      page_scale,
      top_controls_delta);
}

void RenderWidgetCompositor::RequestNewOutputSurface(bool fallback) {
  layer_tree_host_->SetOutputSurface(widget_->CreateOutputSurface(fallback));
}

void RenderWidgetCompositor::DidInitializeOutputSurface() {
}

void RenderWidgetCompositor::WillCommit() {
  widget_->InstrumentWillComposite();
}

void RenderWidgetCompositor::DidCommit() {
  if (send_v8_idle_notification_after_commit_) {
    base::TimeDelta idle_time = begin_main_frame_time_ +
                                begin_main_frame_interval_ -
                                gfx::FrameTime::Now();
    if (idle_time > base::TimeDelta()) {
      // Convert to 32-bit microseconds first to avoid costly 64-bit division.
      int32 idle_time_in_us = idle_time.InMicroseconds();
      int32 idle_time_in_ms = idle_time_in_us / 1000;
      if (idle_time_in_ms)
        blink::mainThreadIsolate()->IdleNotification(idle_time_in_ms);
    }
  }

  widget_->DidCommitCompositorFrame();
  widget_->didBecomeReadyForAdditionalInput();
  widget_->webwidget()->didCommitFrameToCompositor();
}

void RenderWidgetCompositor::DidCommitAndDrawFrame() {
  widget_->didCommitAndDrawCompositorFrame();
}

void RenderWidgetCompositor::DidCompleteSwapBuffers() {
  widget_->didCompleteSwapBuffers();
  if (!threaded_)
    widget_->OnSwapBuffersComplete();
}

void RenderWidgetCompositor::ScheduleAnimation() {
  widget_->scheduleAnimation();
}

void RenderWidgetCompositor::DidPostSwapBuffers() {
  widget_->OnSwapBuffersPosted();
}

void RenderWidgetCompositor::DidAbortSwapBuffers() {
  widget_->OnSwapBuffersAborted();
}

void RenderWidgetCompositor::RateLimitSharedMainThreadContext() {
  cc::ContextProvider* provider =
      RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
  provider->ContextGL()->RateLimitOffscreenContextCHROMIUM();
}

}  // namespace content
