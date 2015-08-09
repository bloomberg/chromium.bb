// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <limits>
#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/switches.h"
#include "cc/blink/web_compositor_animation_timeline_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/single_release_callback.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/common/content_switches_internal.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/input/input_handler_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebLayoutAndPaintAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSelection.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
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
using blink::WebSelection;
using blink::WebSize;
using blink::WebTopControlsState;

namespace content {
namespace {

bool GetSwitchValueAsInt(const base::CommandLine& command_line,
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
    const WebSelection& web_selection,
    bool is_start) {
  cc::LayerSelectionBound cc_bound;
  if (web_selection.isNone())
    return cc_bound;

  const blink::WebSelectionBound& web_bound =
      is_start ? web_selection.start() : web_selection.end();
  DCHECK(web_bound.layerId);
  cc_bound.type = cc::SELECTION_BOUND_CENTER;
  if (web_selection.isRange()) {
    if (is_start) {
      cc_bound.type = web_bound.isTextDirectionRTL ? cc::SELECTION_BOUND_RIGHT
                                                   : cc::SELECTION_BOUND_LEFT;
    } else {
      cc_bound.type = web_bound.isTextDirectionRTL ? cc::SELECTION_BOUND_LEFT
                                                   : cc::SELECTION_BOUND_RIGHT;
    }
  }
  cc_bound.layer_id = web_bound.layerId;
  cc_bound.edge_top = gfx::Point(web_bound.edgeTopInLayer);
  cc_bound.edge_bottom = gfx::Point(web_bound.edgeBottomInLayer);
  return cc_bound;
}

cc::LayerSelection ConvertWebSelection(const WebSelection& web_selection) {
  cc::LayerSelection cc_selection;
  cc_selection.start = ConvertWebSelectionBound(web_selection, true);
  cc_selection.end = ConvertWebSelectionBound(web_selection, false);
  cc_selection.is_editable = web_selection.isEditable();
  cc_selection.is_empty_text_form_control =
      web_selection.isEmptyTextFormControl();
  return cc_selection;
}

gfx::Size CalculateDefaultTileSize(RenderWidget* widget) {
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
#elif defined(OS_CHROMEOS)
  // Use 512 for high DPI (dsf=2.0f) devices.
  if (widget->screen_info().deviceScaleFactor >= 2.0f)
    default_tile_size = 512;
#endif

  return gfx::Size(default_tile_size, default_tile_size);
}

// Check cc::TopControlsState, and blink::WebTopControlsState
// are kept in sync.
static_assert(int(blink::WebTopControlsBoth) == int(cc::BOTH),
              "mismatching enums: BOTH");
static_assert(int(blink::WebTopControlsHidden) == int(cc::HIDDEN),
              "mismatching enums: HIDDEN");
static_assert(int(blink::WebTopControlsShown) == int(cc::SHOWN),
              "mismatching enums: SHOWN");

static cc::TopControlsState ConvertTopControlsState(
    WebTopControlsState state) {
  return static_cast<cc::TopControlsState>(state);
}

}  // namespace

// static
scoped_ptr<RenderWidgetCompositor> RenderWidgetCompositor::Create(
    RenderWidget* widget,
    CompositorDependencies* compositor_deps) {
  scoped_ptr<RenderWidgetCompositor> compositor(
      new RenderWidgetCompositor(widget, compositor_deps));
  compositor->Initialize();
  return compositor;
}

RenderWidgetCompositor::RenderWidgetCompositor(
    RenderWidget* widget,
    CompositorDependencies* compositor_deps)
    : num_failed_recreate_attempts_(0),
      widget_(widget),
      compositor_deps_(compositor_deps),
      layout_and_paint_async_callback_(nullptr),
      weak_factory_(this) {
}

void RenderWidgetCompositor::Initialize() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // For web contents, layer transforms should scale up the contents of layers
  // to keep content always crisp when possible.
  settings.layer_transforms_should_scale_layer_contents = true;

  if (cmd->HasSwitch(switches::kDisableGpuVsync)) {
    std::string display_vsync_string =
        cmd->GetSwitchValueASCII(switches::kDisableGpuVsync);
    if (display_vsync_string == "gpu") {
      settings.renderer_settings.disable_display_vsync = true;
    } else if (display_vsync_string == "beginframe") {
      settings.wait_for_beginframe_interval = false;
    } else {
      settings.renderer_settings.disable_display_vsync = true;
      settings.wait_for_beginframe_interval = false;
    }
  }
  settings.main_frame_before_activation_enabled =
      cmd->HasSwitch(cc::switches::kEnableMainFrameBeforeActivation) &&
      !cmd->HasSwitch(cc::switches::kDisableMainFrameBeforeActivation);
  settings.accelerated_animation_enabled =
      !cmd->HasSwitch(cc::switches::kDisableThreadedAnimation);
  const std::string slimming_group =
      base::FieldTrialList::FindFullName("SlimmingPaint");
  settings.use_display_lists =
      (cmd->HasSwitch(switches::kEnableSlimmingPaint) ||
      !cmd->HasSwitch(switches::kDisableSlimmingPaint)) &&
      (slimming_group != "DisableSlimmingPaint");
  if (cmd->HasSwitch(switches::kEnableCompositorAnimationTimelines)) {
    settings.use_compositor_animation_timelines = true;
    blink::WebRuntimeFeatures::enableCompositorAnimationTimelines(true);
  }

  settings.default_tile_size = CalculateDefaultTileSize(widget_);
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

  settings.gpu_rasterization_msaa_sample_count =
      compositor_deps_->GetGpuRasterizationMSAASampleCount();
  settings.gpu_rasterization_forced =
      compositor_deps_->IsGpuRasterizationForced();
  settings.gpu_rasterization_enabled =
      compositor_deps_->IsGpuRasterizationEnabled();

  settings.can_use_lcd_text = compositor_deps_->IsLcdTextEnabled();
  settings.use_distance_field_text =
      compositor_deps_->IsDistanceFieldTextEnabled();
  settings.use_zero_copy = compositor_deps_->IsZeroCopyEnabled();
  settings.use_one_copy = compositor_deps_->IsOneCopyEnabled();
  settings.enable_elastic_overscroll =
      compositor_deps_->IsElasticOverscrollEnabled();
  settings.use_image_texture_targets =
      compositor_deps_->GetImageTextureTargets();
  settings.gather_pixel_refs = compositor_deps_->IsGatherPixelRefsEnabled();

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

  settings.verify_property_trees =
      cmd->HasSwitch(cc::switches::kEnablePropertyTreeVerification);
  settings.renderer_settings.allow_antialiasing &=
      !cmd->HasSwitch(cc::switches::kDisableCompositedAntialiasing);
  // The means the renderer compositor has 2 possible modes:
  // - Threaded compositing with a scheduler.
  // - Single threaded compositing without a scheduler (for layout tests only).
  // Using the scheduler in layout tests introduces additional composite steps
  // that create flakiness.
  settings.single_thread_proxy_scheduler = false;

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

  settings.invert_viewport_scroll_order =
      cmd->HasSwitch(switches::kInvertViewportScrollOrder);

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

  // We can't use GPU rasterization on low-end devices, because the Ganesh
  // cache would consume too much memory.
  if (base::SysInfo::IsLowEndDevice())
    settings.gpu_rasterization_enabled = false;
  settings.using_synchronous_renderer_compositor =
      synchronous_compositor_factory;
  settings.record_full_layer = widget_->DoesRecordFullLayer();
  settings.max_partial_texture_updates = 0;
  if (synchronous_compositor_factory) {
    // Android WebView uses system scrollbars, so make ours invisible.
    settings.scrollbar_animator = cc::LayerTreeSettings::NO_ANIMATOR;
    settings.solid_color_scrollbar_color = SK_ColorTRANSPARENT;
  } else {
    settings.scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
    settings.scrollbar_fade_delay_ms = 300;
    settings.scrollbar_fade_resize_delay_ms = 2000;
    settings.scrollbar_fade_duration_ms = 300;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings.renderer_settings.highp_threshold_min = 2048;
  // Android WebView handles root layer flings itself.
  settings.ignore_root_layer_flings =
      synchronous_compositor_factory;
  // Memory policy on Android WebView does not depend on whether device is
  // low end, so always use default policy.
  bool use_low_memory_policy =
      base::SysInfo::IsLowEndDevice() && !synchronous_compositor_factory;
  // RGBA_4444 textures are only enabled for low end devices
  // and are disabled for Android WebView as it doesn't support the format.
  settings.renderer_settings.use_rgba_4444_textures = use_low_memory_policy;
  if (use_low_memory_policy) {
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
  settings.renderer_settings.should_clear_root_render_pass =
      !synchronous_compositor_factory;

  // TODO(danakj): Only do this on low end devices.
  settings.create_low_res_tiling = true;

  settings.use_external_begin_frame_source = true;

#elif !defined(OS_MACOSX)
  if (ui::IsOverlayScrollbarEnabled()) {
    settings.scrollbar_animator = cc::LayerTreeSettings::THINNING;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  } else {
    settings.scrollbar_animator = cc::LayerTreeSettings::LINEAR_FADE;
    settings.solid_color_scrollbar_color = SkColorSetARGB(128, 128, 128, 128);
  }
  settings.scrollbar_fade_delay_ms = 500;
  settings.scrollbar_fade_resize_delay_ms = 500;
  settings.scrollbar_fade_duration_ms = 300;
#endif

  if (cmd->HasSwitch(switches::kEnableLowResTiling))
    settings.create_low_res_tiling = true;
  if (cmd->HasSwitch(switches::kDisableLowResTiling))
    settings.create_low_res_tiling = false;
  if (cmd->HasSwitch(cc::switches::kEnableBeginFrameScheduling))
    settings.use_external_begin_frame_source = true;

  if (widget_->for_oopif()) {
    // TODO(simonhong): Apply BeginFrame scheduling for OOPIF.
    // See crbug.com/471411.
    settings.use_external_begin_frame_source = false;
  }

  settings.max_staging_buffers = 32;
  // Use 1/4th of staging buffers on low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    settings.max_staging_buffers /= 4;

  scoped_refptr<base::SingleThreadTaskRunner> compositor_thread_task_runner =
      compositor_deps_->GetCompositorImplThreadTaskRunner();
  scoped_refptr<base::SingleThreadTaskRunner>
      main_thread_compositor_task_runner =
          compositor_deps_->GetCompositorMainThreadTaskRunner();
  cc::SharedBitmapManager* shared_bitmap_manager =
      compositor_deps_->GetSharedBitmapManager();
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      compositor_deps_->GetGpuMemoryBufferManager();
  cc::TaskGraphRunner* task_graph_runner =
      compositor_deps_->GetTaskGraphRunner();

  scoped_ptr<cc::BeginFrameSource> external_begin_frame_source;
  if (settings.use_external_begin_frame_source) {
    external_begin_frame_source =
        compositor_deps_->CreateExternalBeginFrameSource(widget_->routing_id());
  }

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.shared_bitmap_manager = shared_bitmap_manager;
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.settings = &settings;
  params.task_graph_runner = task_graph_runner;
  params.main_task_runner = main_thread_compositor_task_runner;
  params.external_begin_frame_source = external_begin_frame_source.Pass();
  if (compositor_thread_task_runner.get()) {
    layer_tree_host_ = cc::LayerTreeHost::CreateThreaded(
        compositor_thread_task_runner, &params);
  } else {
    layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  }
  DCHECK(layer_tree_host_);
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

int RenderWidgetCompositor::GetSourceFrameNumber() const {
  return layer_tree_host_->source_frame_number();
}

void RenderWidgetCompositor::SetNeedsUpdateLayers() {
  layer_tree_host_->SetNeedsUpdateLayers();
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

void RenderWidgetCompositor::StartCompositor() {
  layer_tree_host_->SetLayerTreeHostClientReady();
}

void RenderWidgetCompositor::setRootLayer(const blink::WebLayer& layer) {
  layer_tree_host_->SetRootLayer(
      static_cast<const cc_blink::WebLayerImpl*>(&layer)->layer());
}

void RenderWidgetCompositor::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::attachCompositorAnimationTimeline(
    blink::WebCompositorAnimationTimeline* compositor_timeline) {
  DCHECK(compositor_timeline);
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->AddAnimationTimeline(
      static_cast<const cc_blink::WebCompositorAnimationTimelineImpl*>(
          compositor_timeline)->animation_timeline());
}

void RenderWidgetCompositor::detachCompositorAnimationTimeline(
    blink::WebCompositorAnimationTimeline* compositor_timeline) {
  DCHECK(compositor_timeline);
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->RemoveAnimationTimeline(
      static_cast<const cc_blink::WebCompositorAnimationTimelineImpl*>(
          compositor_timeline)->animation_timeline());
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

void RenderWidgetCompositor::didStopFlinging() {
  layer_tree_host_->DidStopFlinging();
}

void RenderWidgetCompositor::registerForAnimations(blink::WebLayer* layer) {
  cc::Layer* cc_layer = static_cast<cc_blink::WebLayerImpl*>(layer)->layer();
  cc_layer->RegisterForAnimations(layer_tree_host_->animation_registrar());
}

void RenderWidgetCompositor::registerViewportLayers(
    const blink::WebLayer* overscrollElasticityLayer,
    const blink::WebLayer* pageScaleLayer,
    const blink::WebLayer* innerViewportScrollLayer,
    const blink::WebLayer* outerViewportScrollLayer) {
  layer_tree_host_->RegisterViewportLayers(
      // TODO(bokan): This check can probably be removed now, but it looks
      // like overscroll elasticity may still be NULL until PinchViewport
      // registers its layers.
      // The scroll elasticity layer will only exist when using pinch virtual
      // viewports.
      overscrollElasticityLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(
                overscrollElasticityLayer)->layer()
          : NULL,
      static_cast<const cc_blink::WebLayerImpl*>(pageScaleLayer)->layer(),
      static_cast<const cc_blink::WebLayerImpl*>(innerViewportScrollLayer)
          ->layer(),
      // TODO(bokan): This check can probably be removed now, but it looks
      // like overscroll elasticity may still be NULL until PinchViewport
      // registers its layers.
      // The outer viewport layer will only exist when using pinch virtual
      // viewports.
      outerViewportScrollLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(outerViewportScrollLayer)
                ->layer()
          : NULL);
}

void RenderWidgetCompositor::clearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(
      scoped_refptr<cc::Layer>(), scoped_refptr<cc::Layer>(),
      scoped_refptr<cc::Layer>(), scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::registerSelection(
    const blink::WebSelection& selection) {
  layer_tree_host_->RegisterSelection(ConvertWebSelection(selection));
}

void RenderWidgetCompositor::clearSelection() {
  cc::LayerSelection empty_selection;
  layer_tree_host_->RegisterSelection(empty_selection);
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

bool RenderWidgetCompositor::CompositeIsSynchronous() const {
  return !compositor_deps_->GetCompositorImplThreadTaskRunner().get() &&
         !layer_tree_host_->settings().single_thread_proxy_scheduler;
}

void RenderWidgetCompositor::layoutAndPaintAsync(
    blink::WebLayoutAndPaintAsyncCallback* callback) {
  DCHECK(!temporary_copy_output_request_ && !layout_and_paint_async_callback_);
  layout_and_paint_async_callback_ = callback;

  if (CompositeIsSynchronous()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RenderWidgetCompositor::LayoutAndUpdateLayers,
                              weak_factory_.GetWeakPtr()));
  } else {
    layer_tree_host_->SetNeedsCommit();
  }
}

void RenderWidgetCompositor::LayoutAndUpdateLayers() {
  DCHECK(CompositeIsSynchronous());
  layer_tree_host_->LayoutAndUpdateLayers();
  InvokeLayoutAndPaintCallback();
}

void RenderWidgetCompositor::InvokeLayoutAndPaintCallback() {
  if (!layout_and_paint_async_callback_)
    return;
  layout_and_paint_async_callback_->didLayoutAndPaint();
  layout_and_paint_async_callback_ = nullptr;
}

void RenderWidgetCompositor::compositeAndReadbackAsync(
    blink::WebCompositeAndReadbackAsyncCallback* callback) {
  DCHECK(!temporary_copy_output_request_ && !layout_and_paint_async_callback_);
  temporary_copy_output_request_ =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&CompositeAndReadbackAsyncCallback, callback));

  // Force a commit to happen. The temporary copy output request will
  // be installed after layout which will happen as a part of the commit, for
  // widgets that delay the creation of their output surface.
  if (CompositeIsSynchronous()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RenderWidgetCompositor::SynchronouslyComposite,
                              weak_factory_.GetWeakPtr()));
  } else {
    layer_tree_host_->SetNeedsCommit();
  }
}

void RenderWidgetCompositor::SynchronouslyComposite() {
  DCHECK(CompositeIsSynchronous());
  layer_tree_host_->Composite(base::TimeTicks::Now());
}

void RenderWidgetCompositor::finishAllRendering() {
  layer_tree_host_->FinishAllRendering();
}

void RenderWidgetCompositor::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

int RenderWidgetCompositor::layerTreeId() const {
  return layer_tree_host_->id();
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

void RenderWidgetCompositor::updateTopControlsState(
    WebTopControlsState constraints,
    WebTopControlsState current,
    bool animate) {
  layer_tree_host_->UpdateTopControlsState(ConvertTopControlsState(constraints),
                                           ConvertTopControlsState(current),
                                           animate);
}

void RenderWidgetCompositor::setTopControlsHeight(float height, bool shrink) {
    layer_tree_host_->SetTopControlsHeight(height, shrink);
}

void RenderWidgetCompositor::setTopControlsShownRatio(float ratio) {
  layer_tree_host_->SetTopControlsShownRatio(ratio);
}

void RenderWidgetCompositor::setHidePinchScrollbarsNearMinScale(bool hide) {
  layer_tree_host_->set_hide_pinch_scrollbars_near_min_scale(hide);
}

void RenderWidgetCompositor::WillBeginMainFrame() {
  widget_->WillBeginCompositorFrame();
}

void RenderWidgetCompositor::DidBeginMainFrame() {
}

void RenderWidgetCompositor::BeginMainFrame(const cc::BeginFrameArgs& args) {
  double frame_time_sec = (args.frame_time - base::TimeTicks()).InSecondsF();
  double deadline_sec = (args.deadline - base::TimeTicks()).InSecondsF();
  double interval_sec = args.interval.InSecondsF();
  WebBeginFrameArgs web_begin_frame_args =
      WebBeginFrameArgs(frame_time_sec, deadline_sec, interval_sec);
  compositor_deps_->GetRendererScheduler()->WillBeginFrame(args);
  widget_->webwidget()->beginFrame(web_begin_frame_args);
}

void RenderWidgetCompositor::BeginMainFrameNotExpectedSoon() {
  compositor_deps_->GetRendererScheduler()->BeginFrameNotExpectedSoon();
}

void RenderWidgetCompositor::Layout() {
  widget_->webwidget()->layout();

  if (temporary_copy_output_request_) {
    // For WebViewImpl, this will always have a root layer.  For other widgets,
    // the widget may be closed before servicing this request, so ignore it.
    if (cc::Layer* root_layer = layer_tree_host_->root_layer()) {
      root_layer->RequestCopyOfOutput(temporary_copy_output_request_.Pass());
    } else {
      temporary_copy_output_request_->SendEmptyResult();
      temporary_copy_output_request_ = nullptr;
    }
  }
}

void RenderWidgetCompositor::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  widget_->webwidget()->applyViewportDeltas(
      inner_delta,
      outer_delta,
      elastic_overscroll_delta,
      page_scale,
      top_controls_delta);
}

void RenderWidgetCompositor::RequestNewOutputSurface() {
  // If the host is closing, then no more compositing is possible.  This
  // prevents shutdown races between handling the close message and
  // the CreateOutputSurface task.
  if (widget_->host_closing())
    return;

  bool fallback =
      num_failed_recreate_attempts_ >= OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK;
  scoped_ptr<cc::OutputSurface> surface(widget_->CreateOutputSurface(fallback));

  if (!surface) {
    DidFailToInitializeOutputSurface();
    return;
  }

  DCHECK_EQ(surface->capabilities().max_frames_pending, 1);

  layer_tree_host_->SetOutputSurface(surface.Pass());
}

void RenderWidgetCompositor::DidInitializeOutputSurface() {
  num_failed_recreate_attempts_ = 0;
}

void RenderWidgetCompositor::DidFailToInitializeOutputSurface() {
  ++num_failed_recreate_attempts_;
  // Tolerate a certain number of recreation failures to work around races
  // in the output-surface-lost machinery.
  LOG_IF(FATAL, (num_failed_recreate_attempts_ >= MAX_OUTPUT_SURFACE_RETRIES))
      << "Failed to create a fallback OutputSurface.";

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderWidgetCompositor::RequestNewOutputSurface,
                            weak_factory_.GetWeakPtr()));
}

void RenderWidgetCompositor::WillCommit() {
  InvokeLayoutAndPaintCallback();
}

void RenderWidgetCompositor::DidCommit() {
  DCHECK(!temporary_copy_output_request_);
  widget_->DidCommitCompositorFrame();
  compositor_deps_->GetRendererScheduler()->DidCommitFrameToCompositor();
}

void RenderWidgetCompositor::DidCommitAndDrawFrame() {
  widget_->DidCommitAndDrawCompositorFrame();
}

void RenderWidgetCompositor::DidCompleteSwapBuffers() {
  widget_->DidCompleteSwapBuffers();
  bool threaded = !!compositor_deps_->GetCompositorImplThreadTaskRunner().get();
  if (!threaded)
    widget_->OnSwapBuffersComplete();
}

void RenderWidgetCompositor::DidCompletePageScaleAnimation() {
  widget_->DidCompletePageScaleAnimation();
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

void RenderWidgetCompositor::RecordFrameTimingEvents(
    scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events) {
  for (const auto& composite_event : *composite_events ) {
    int64_t frameId = composite_event.first;
    const std::vector<cc::FrameTimingTracker::CompositeTimingEvent>& events =
        composite_event.second;
    std::vector<blink::WebFrameTimingEvent> webEvents;
    for (size_t i = 0; i < events.size(); ++i) {
      webEvents.push_back(blink::WebFrameTimingEvent(
          events[i].frame_id,
          (events[i].timestamp - base::TimeTicks()).InSecondsF()));
    }
    widget_->webwidget()->recordFrameTimingEvent(
        blink::WebWidget::CompositeEvent, frameId, webEvents);
  }
  for (const auto& main_frame_event : *main_frame_events ) {
    int64_t frameId = main_frame_event.first;
    const std::vector<cc::FrameTimingTracker::MainFrameTimingEvent>& events =
        main_frame_event.second;
    std::vector<blink::WebFrameTimingEvent> webEvents;
    for (size_t i = 0; i < events.size(); ++i) {
      webEvents.push_back(blink::WebFrameTimingEvent(
          events[i].frame_id,
          (events[i].timestamp - base::TimeTicks()).InSecondsF(),
          (events[i].end_time - base::TimeTicks()).InSecondsF()));
    }
    widget_->webwidget()->recordFrameTimingEvent(
        blink::WebWidget::RenderEvent, frameId, webEvents);
  }
}

void RenderWidgetCompositor::RateLimitSharedMainThreadContext() {
  cc::ContextProvider* provider =
      compositor_deps_->GetSharedMainThreadContextProvider();
  // provider can be NULL after the GPU process crashed enough times and we
  // don't want to restart it any more (falling back to software).
  if (!provider)
    return;
  provider->ContextGL()->RateLimitOffscreenContextCHROMIUM();
}

void RenderWidgetCompositor::SetSurfaceIdNamespace(
    uint32_t surface_id_namespace) {
  layer_tree_host_->set_surface_id_namespace(surface_id_namespace);
}

}  // namespace content
