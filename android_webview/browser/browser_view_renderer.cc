// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"

#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/child_frame.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/compositor_frame.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace android_webview {

namespace {

const double kEpsilon = 1e-8;

const int64 kFallbackTickTimeoutInMilliseconds = 100;

// Used to calculate memory allocation. Determined experimentally.
const size_t kMemoryMultiplier = 20;
const size_t kBytesPerPixel = 4;
const size_t kMemoryAllocationStep = 5 * 1024 * 1024;
uint64_t g_memory_override_in_bytes = 0u;

const void* const kBrowserViewRendererUserDataKey =
    &kBrowserViewRendererUserDataKey;

class BrowserViewRendererUserData : public base::SupportsUserData::Data {
 public:
  explicit BrowserViewRendererUserData(BrowserViewRenderer* ptr) : bvr_(ptr) {}

  static BrowserViewRenderer* GetBrowserViewRenderer(
      content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    BrowserViewRendererUserData* data =
        static_cast<BrowserViewRendererUserData*>(
            web_contents->GetUserData(kBrowserViewRendererUserDataKey));
    return data ? data->bvr_ : NULL;
  }

 private:
  BrowserViewRenderer* bvr_;
};

}  // namespace

// static
void BrowserViewRenderer::CalculateTileMemoryPolicy() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  // If the value was overridden on the command line, use the specified value.
  bool client_hard_limit_bytes_overridden =
      cl->HasSwitch(switches::kForceGpuMemAvailableMb);
  if (client_hard_limit_bytes_overridden) {
    base::StringToUint64(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceGpuMemAvailableMb),
        &g_memory_override_in_bytes);
    g_memory_override_in_bytes *= 1024 * 1024;
  }
}

// static
BrowserViewRenderer* BrowserViewRenderer::FromWebContents(
    content::WebContents* web_contents) {
  return BrowserViewRendererUserData::GetBrowserViewRenderer(web_contents);
}

BrowserViewRenderer::BrowserViewRenderer(
    BrowserViewRendererClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    bool disable_page_visibility)
    : client_(client),
      shared_renderer_state_(ui_task_runner, this),
      ui_task_runner_(ui_task_runner),
      disable_page_visibility_(disable_page_visibility),
      compositor_(NULL),
      is_paused_(false),
      view_visible_(false),
      window_visible_(false),
      attached_to_window_(false),
      hardware_enabled_(false),
      dip_scale_(0.f),
      page_scale_factor_(1.f),
      min_page_scale_factor_(0.f),
      max_page_scale_factor_(0.f),
      on_new_picture_enable_(false),
      clear_view_(false),
      offscreen_pre_raster_(false),
      fallback_tick_pending_(false) {}

BrowserViewRenderer::~BrowserViewRenderer() {
}

void BrowserViewRenderer::RegisterWithWebContents(
    content::WebContents* web_contents) {
  web_contents->SetUserData(kBrowserViewRendererUserDataKey,
                            new BrowserViewRendererUserData(this));
}

SharedRendererState* BrowserViewRenderer::GetAwDrawGLViewContext() {
  return &shared_renderer_state_;
}

bool BrowserViewRenderer::RequestDrawGL(bool wait_for_completion) {
  return client_->RequestDrawGL(wait_for_completion);
}

void BrowserViewRenderer::TrimMemory(const int level, const bool visible) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // Constants from Android ComponentCallbacks2.
  enum {
    TRIM_MEMORY_RUNNING_LOW = 10,
    TRIM_MEMORY_UI_HIDDEN = 20,
    TRIM_MEMORY_BACKGROUND = 40,
    TRIM_MEMORY_MODERATE = 60,
  };

  // Not urgent enough. TRIM_MEMORY_UI_HIDDEN is treated specially because
  // it does not indicate memory pressure, but merely that the app is
  // backgrounded.
  if (level < TRIM_MEMORY_RUNNING_LOW || level == TRIM_MEMORY_UI_HIDDEN)
    return;

  // Do not release resources on view we expect to get DrawGL soon.
  if (level < TRIM_MEMORY_BACKGROUND && visible)
    return;

  // Nothing to drop.
  if (!compositor_ || !hardware_enabled_)
    return;

  TRACE_EVENT0("android_webview", "BrowserViewRenderer::TrimMemory");

  // If offscreen pre-raster is disabled, drop everything in hardware. Otherwise
  // keep the tiles and just delete the HardwareRenderer.
  if (level >= TRIM_MEMORY_MODERATE) {
    if (offscreen_pre_raster_)
      shared_renderer_state_.DeleteHardwareRendererOnUI();
    else
      shared_renderer_state_.ReleaseHardwareDrawIfNeededOnUI();
    return;
  }

  // Just set the memory limit to 0 and drop all tiles. This will be reset to
  // normal levels in the next DrawGL call.
  if (!offscreen_pre_raster_)
    compositor_->SetMemoryPolicy(0u);
}

void BrowserViewRenderer::UpdateMemoryPolicy() {
  if (!hardware_enabled_) {
    compositor_->SetMemoryPolicy(0u);
    return;
  }

  size_t bytes_limit = 0u;
  if (g_memory_override_in_bytes) {
    bytes_limit = static_cast<size_t>(g_memory_override_in_bytes);
  } else {
    ParentCompositorDrawConstraints parent_draw_constraints =
        shared_renderer_state_.GetParentDrawConstraintsOnUI();
    gfx::Rect interest_rect =
        offscreen_pre_raster_ || parent_draw_constraints.is_layer
            ? gfx::Rect(size_)
            : last_on_draw_global_visible_rect_;
    size_t width = interest_rect.width();
    size_t height = interest_rect.height();
    bytes_limit = kMemoryMultiplier * kBytesPerPixel * width * height;
    // Round up to a multiple of kMemoryAllocationStep.
    bytes_limit =
        (bytes_limit / kMemoryAllocationStep + 1) * kMemoryAllocationStep;
  }

  compositor_->SetMemoryPolicy(bytes_limit);
}

void BrowserViewRenderer::PrepareToDraw(const gfx::Vector2d& scroll,
                                        const gfx::Rect& global_visible_rect) {
  last_on_draw_scroll_offset_ = scroll;
  last_on_draw_global_visible_rect_ = global_visible_rect;
}

bool BrowserViewRenderer::CanOnDraw() {
  if (!compositor_) {
    TRACE_EVENT_INSTANT0("android_webview", "EarlyOut_NoCompositor",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (clear_view_) {
    TRACE_EVENT_INSTANT0("android_webview", "EarlyOut_ClearView",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return true;
}

bool BrowserViewRenderer::OnDrawHardware() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDrawHardware");

  shared_renderer_state_.InitializeHardwareDrawIfNeededOnUI();

  if (!CanOnDraw()) {
    return false;
  }

  shared_renderer_state_.SetScrollOffsetOnUI(last_on_draw_scroll_offset_);
  hardware_enabled_ = true;

  return CompositeHw();
}

bool BrowserViewRenderer::CompositeHw() {
  CancelFallbackTick();

  ReturnResourceFromParent();
  UpdateMemoryPolicy();

  ParentCompositorDrawConstraints parent_draw_constraints =
      shared_renderer_state_.GetParentDrawConstraintsOnUI();
  gfx::Size surface_size(size_);
  gfx::Rect viewport(surface_size);
  gfx::Rect clip = viewport;
  gfx::Transform transform_for_tile_priority =
      parent_draw_constraints.transform;

  // If the WebView is on a layer, WebView does not know what transform is
  // applied onto the layer so global visible rect does not make sense here.
  // In this case, just use the surface rect for tiling.
  gfx::Rect viewport_rect_for_tile_priority;

  // Leave viewport_rect_for_tile_priority empty if offscreen_pre_raster_ is on.
  if (!offscreen_pre_raster_ && !parent_draw_constraints.is_layer) {
    viewport_rect_for_tile_priority = last_on_draw_global_visible_rect_;
  }

  scoped_ptr<cc::CompositorFrame> frame =
      compositor_->DemandDrawHw(surface_size,
                                gfx::Transform(),
                                viewport,
                                clip,
                                viewport_rect_for_tile_priority,
                                transform_for_tile_priority);
  if (!frame.get()) {
    TRACE_EVENT_INSTANT0("android_webview", "NoNewFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    return shared_renderer_state_.HasFrameOnUI();
  }

  scoped_ptr<ChildFrame> child_frame = make_scoped_ptr(
      new ChildFrame(frame.Pass(), viewport_rect_for_tile_priority.IsEmpty(),
                     transform_for_tile_priority, offscreen_pre_raster_,
                     parent_draw_constraints.is_layer));

  // Uncommitted frame can happen with consecutive fallback ticks.
  ReturnUnusedResource(shared_renderer_state_.PassUncommittedFrameOnUI());
  shared_renderer_state_.SetCompositorFrameOnUI(child_frame.Pass());
  return true;
}

void BrowserViewRenderer::UpdateParentDrawConstraints() {
  PostInvalidateWithFallback();
  ParentCompositorDrawConstraints parent_draw_constraints =
      shared_renderer_state_.GetParentDrawConstraintsOnUI();
  client_->ParentDrawConstraintsUpdated(parent_draw_constraints);
}

void BrowserViewRenderer::ReturnUnusedResource(
    scoped_ptr<ChildFrame> child_frame) {
  if (!child_frame.get() || !child_frame->frame.get())
    return;

  cc::CompositorFrameAck frame_ack;
  cc::TransferableResource::ReturnResources(
      child_frame->frame->delegated_frame_data->resource_list,
      &frame_ack.resources);
  if (compositor_ && !frame_ack.resources.empty())
    compositor_->ReturnResources(frame_ack);
}

void BrowserViewRenderer::ReturnResourceFromParent() {
  cc::CompositorFrameAck frame_ack;
  shared_renderer_state_.SwapReturnedResourcesOnUI(&frame_ack.resources);
  if (compositor_ && !frame_ack.resources.empty()) {
    compositor_->ReturnResources(frame_ack);
  }
}

void BrowserViewRenderer::DetachFunctorFromView() {
  client_->DetachFunctorFromView();
}

bool BrowserViewRenderer::OnDrawSoftware(SkCanvas* canvas) {
  return CanOnDraw() && CompositeSW(canvas);
}

skia::RefPtr<SkPicture> BrowserViewRenderer::CapturePicture(int width,
                                                            int height) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::CapturePicture");

  // Return empty Picture objects for empty SkPictures.
  if (width <= 0 || height <= 0) {
    SkPictureRecorder emptyRecorder;
    emptyRecorder.beginRecording(0, 0);
    return skia::AdoptRef(emptyRecorder.endRecording());
  }

  SkPictureRecorder recorder;
  SkCanvas* rec_canvas = recorder.beginRecording(width, height, NULL, 0);
  if (compositor_) {
    {
      // Reset scroll back to the origin, will go back to the old
      // value when scroll_reset is out of scope.
      compositor_->DidChangeRootLayerScrollOffset(gfx::ScrollOffset());
      CompositeSW(rec_canvas);
    }
    compositor_->DidChangeRootLayerScrollOffset(
        gfx::ScrollOffset(scroll_offset_dip_));
  }
  return skia::AdoptRef(recorder.endRecording());
}

void BrowserViewRenderer::EnableOnNewPicture(bool enabled) {
  on_new_picture_enable_ = enabled;
}

void BrowserViewRenderer::ClearView() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::ClearView",
                       TRACE_EVENT_SCOPE_THREAD);
  if (clear_view_)
    return;

  clear_view_ = true;
  // Always invalidate ignoring the compositor to actually clear the webview.
  PostInvalidateWithFallback();
}

void BrowserViewRenderer::SetOffscreenPreRaster(bool enable) {
  if (offscreen_pre_raster_ != enable && compositor_)
    UpdateMemoryPolicy();

  offscreen_pre_raster_ = enable;
}

void BrowserViewRenderer::SetIsPaused(bool paused) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetIsPaused",
                       TRACE_EVENT_SCOPE_THREAD,
                       "paused",
                       paused);
  is_paused_ = paused;
  UpdateCompositorIsActive();
}

void BrowserViewRenderer::SetViewVisibility(bool view_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetViewVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "view_visible",
                       view_visible);
  view_visible_ = view_visible;
}

void BrowserViewRenderer::SetWindowVisibility(bool window_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetWindowVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "window_visible",
                       window_visible);
  window_visible_ = window_visible;
  UpdateCompositorIsActive();
}

void BrowserViewRenderer::OnSizeChanged(int width, int height) {
  TRACE_EVENT_INSTANT2("android_webview",
                       "BrowserViewRenderer::OnSizeChanged",
                       TRACE_EVENT_SCOPE_THREAD,
                       "width",
                       width,
                       "height",
                       height);
  size_.SetSize(width, height);
}

void BrowserViewRenderer::OnAttachedToWindow(int width, int height) {
  TRACE_EVENT2("android_webview",
               "BrowserViewRenderer::OnAttachedToWindow",
               "width",
               width,
               "height",
               height);
  attached_to_window_ = true;
  size_.SetSize(width, height);
  UpdateCompositorIsActive();
}

void BrowserViewRenderer::OnDetachedFromWindow() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDetachedFromWindow");
  shared_renderer_state_.ReleaseHardwareDrawIfNeededOnUI();
  attached_to_window_ = false;
  DCHECK(!hardware_enabled_);
  UpdateCompositorIsActive();
}

void BrowserViewRenderer::OnComputeScroll(base::TimeTicks animation_time) {
  if (!compositor_)
    return;
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnComputeScroll");
  compositor_->OnComputeScroll(animation_time);
}

void BrowserViewRenderer::ReleaseHardware() {
  DCHECK(hardware_enabled_);
  ReturnUnusedResource(shared_renderer_state_.PassUncommittedFrameOnUI());
  ReturnResourceFromParent();
  DCHECK(shared_renderer_state_.ReturnedResourcesEmptyOnUI());

  if (compositor_) {
    compositor_->SetMemoryPolicy(0u);
  }

  hardware_enabled_ = false;
}

bool BrowserViewRenderer::IsVisible() const {
  // Ignore |window_visible_| if |attached_to_window_| is false.
  return view_visible_ && (!attached_to_window_ || window_visible_);
}

bool BrowserViewRenderer::IsClientVisible() const {
  if (disable_page_visibility_)
    return !is_paused_;

  return !is_paused_ && (!attached_to_window_ || window_visible_);
}

gfx::Rect BrowserViewRenderer::GetScreenRect() const {
  return gfx::Rect(client_->GetLocationOnScreen(), size_);
}

void BrowserViewRenderer::DidInitializeCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview",
               "BrowserViewRenderer::DidInitializeCompositor");
  DCHECK(compositor);
  DCHECK(!compositor_);
  compositor_ = compositor;
  UpdateCompositorIsActive();
}

void BrowserViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::DidDestroyCompositor");
  DCHECK(compositor_);
  compositor_->SetIsActive(false);
  compositor_ = NULL;
}

void BrowserViewRenderer::SetDipScale(float dip_scale) {
  dip_scale_ = dip_scale;
  CHECK_GT(dip_scale_, 0.f);
}

gfx::Vector2d BrowserViewRenderer::max_scroll_offset() const {
  DCHECK_GT(dip_scale_, 0.f);
  return gfx::ToCeiledVector2d(gfx::ScaleVector2d(
      max_scroll_offset_dip_, dip_scale_ * page_scale_factor_));
}

void BrowserViewRenderer::ScrollTo(const gfx::Vector2d& scroll_offset) {
  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2dF scroll_offset_dip;
  // To preserve the invariant that scrolling to the maximum physical pixel
  // value also scrolls to the maximum dip pixel value we transform the physical
  // offset into the dip offset by using a proportion (instead of dividing by
  // dip_scale * page_scale_factor).
  if (max_offset.x()) {
    scroll_offset_dip.set_x((scroll_offset.x() * max_scroll_offset_dip_.x()) /
                            max_offset.x());
  }
  if (max_offset.y()) {
    scroll_offset_dip.set_y((scroll_offset.y() * max_scroll_offset_dip_.y()) /
                            max_offset.y());
  }

  DCHECK_LE(0.f, scroll_offset_dip.x());
  DCHECK_LE(0.f, scroll_offset_dip.y());
  DCHECK(scroll_offset_dip.x() < max_scroll_offset_dip_.x() ||
         scroll_offset_dip.x() - max_scroll_offset_dip_.x() < kEpsilon)
      << scroll_offset_dip.x() << " " << max_scroll_offset_dip_.x();
  DCHECK(scroll_offset_dip.y() < max_scroll_offset_dip_.y() ||
         scroll_offset_dip.y() - max_scroll_offset_dip_.y() < kEpsilon)
      << scroll_offset_dip.y() << " " << max_scroll_offset_dip_.y();

  if (scroll_offset_dip_ == scroll_offset_dip)
    return;

  scroll_offset_dip_ = scroll_offset_dip;

  TRACE_EVENT_INSTANT2("android_webview",
               "BrowserViewRenderer::ScrollTo",
               TRACE_EVENT_SCOPE_THREAD,
               "x",
               scroll_offset_dip.x(),
               "y",
               scroll_offset_dip.y());

  if (compositor_) {
    compositor_->DidChangeRootLayerScrollOffset(
        gfx::ScrollOffset(scroll_offset_dip_));
  }
}

void BrowserViewRenderer::DidUpdateContent() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidUpdateContent",
                       TRACE_EVENT_SCOPE_THREAD);
  clear_view_ = false;
  if (on_new_picture_enable_)
    client_->OnNewPicture();
}

void BrowserViewRenderer::SetTotalRootLayerScrollOffset(
    const gfx::Vector2dF& scroll_offset_dip) {
  if (scroll_offset_dip_ == scroll_offset_dip)
    return;
  scroll_offset_dip_ = scroll_offset_dip;

  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2d scroll_offset;
  // For an explanation as to why this is done this way see the comment in
  // BrowserViewRenderer::ScrollTo.
  if (max_scroll_offset_dip_.x()) {
    scroll_offset.set_x((scroll_offset_dip.x() * max_offset.x()) /
                        max_scroll_offset_dip_.x());
  }

  if (max_scroll_offset_dip_.y()) {
    scroll_offset.set_y((scroll_offset_dip.y() * max_offset.y()) /
                        max_scroll_offset_dip_.y());
  }

  DCHECK_LE(0, scroll_offset.x());
  DCHECK_LE(0, scroll_offset.y());
  DCHECK_LE(scroll_offset.x(), max_offset.x());
  DCHECK_LE(scroll_offset.y(), max_offset.y());

  client_->ScrollContainerViewTo(scroll_offset);
}

void BrowserViewRenderer::UpdateRootLayerState(
    const gfx::Vector2dF& total_scroll_offset_dip,
    const gfx::Vector2dF& max_scroll_offset_dip,
    const gfx::SizeF& scrollable_size_dip,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  TRACE_EVENT_INSTANT1(
      "android_webview",
      "BrowserViewRenderer::UpdateRootLayerState",
      TRACE_EVENT_SCOPE_THREAD,
      "state",
      RootLayerStateAsValue(total_scroll_offset_dip, scrollable_size_dip));

  DCHECK_GE(max_scroll_offset_dip.x(), 0.f);
  DCHECK_GE(max_scroll_offset_dip.y(), 0.f);
  DCHECK_GT(page_scale_factor, 0.f);
  // SetDipScale should have been called at least once before this is called.
  DCHECK_GT(dip_scale_, 0.f);

  if (max_scroll_offset_dip_ != max_scroll_offset_dip ||
      scrollable_size_dip_ != scrollable_size_dip ||
      page_scale_factor_ != page_scale_factor ||
      min_page_scale_factor_ != min_page_scale_factor ||
      max_page_scale_factor_ != max_page_scale_factor) {
    max_scroll_offset_dip_ = max_scroll_offset_dip;
    scrollable_size_dip_ = scrollable_size_dip;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;

    client_->UpdateScrollState(max_scroll_offset(), scrollable_size_dip,
                               page_scale_factor, min_page_scale_factor,
                               max_page_scale_factor);
  }
  SetTotalRootLayerScrollOffset(total_scroll_offset_dip);
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
BrowserViewRenderer::RootLayerStateAsValue(
    const gfx::Vector2dF& total_scroll_offset_dip,
    const gfx::SizeF& scrollable_size_dip) {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetDouble("total_scroll_offset_dip.x", total_scroll_offset_dip.x());
  state->SetDouble("total_scroll_offset_dip.y", total_scroll_offset_dip.y());

  state->SetDouble("max_scroll_offset_dip.x", max_scroll_offset_dip_.x());
  state->SetDouble("max_scroll_offset_dip.y", max_scroll_offset_dip_.y());

  state->SetDouble("scrollable_size_dip.width", scrollable_size_dip.width());
  state->SetDouble("scrollable_size_dip.height", scrollable_size_dip.height());

  state->SetDouble("page_scale_factor", page_scale_factor_);
  return state;
}

void BrowserViewRenderer::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity) {
  const float physical_pixel_scale = dip_scale_ * page_scale_factor_;
  if (accumulated_overscroll == latest_overscroll_delta)
    overscroll_rounding_error_ = gfx::Vector2dF();
  gfx::Vector2dF scaled_overscroll_delta =
      gfx::ScaleVector2d(latest_overscroll_delta, physical_pixel_scale);
  gfx::Vector2d rounded_overscroll_delta = gfx::ToRoundedVector2d(
      scaled_overscroll_delta + overscroll_rounding_error_);
  overscroll_rounding_error_ =
      scaled_overscroll_delta - rounded_overscroll_delta;
  gfx::Vector2dF fling_velocity_pixels =
      gfx::ScaleVector2d(current_fling_velocity, physical_pixel_scale);

  client_->DidOverscroll(rounded_overscroll_delta, fling_velocity_pixels);
}

void BrowserViewRenderer::PostInvalidate() {
  TRACE_EVENT_INSTANT0("android_webview", "BrowserViewRenderer::PostInvalidate",
                       TRACE_EVENT_SCOPE_THREAD);
  PostInvalidateWithFallback();
}

void BrowserViewRenderer::PostInvalidateWithFallback() {
  // Always call view invalidate. We rely the Android framework to ignore the
  // invalidate when it's not needed such as when view is not visible.
  client_->PostInvalidate();

  // Stop fallback ticks when one of these is true.
  // 1) Webview is paused. Also need to check we are not in clear view since
  // paused, offscreen still expect clear view to recover.
  // 2) If we are attached to window and the window is not visible (eg when
  // app is in the background). We are sure in this case the webview is used
  // "on-screen" but that updates are not needed when in the background.
  bool throttle_fallback_tick =
      (is_paused_ && !clear_view_) || (attached_to_window_ && !window_visible_);

  if (throttle_fallback_tick || fallback_tick_pending_)
    return;

  DCHECK(post_fallback_tick_.IsCancelled());
  DCHECK(fallback_tick_fired_.IsCancelled());

  post_fallback_tick_.Reset(base::Bind(&BrowserViewRenderer::PostFallbackTick,
                                       base::Unretained(this)));
  ui_task_runner_->PostTask(FROM_HERE, post_fallback_tick_.callback());
  fallback_tick_pending_ = true;
}

void BrowserViewRenderer::CancelFallbackTick() {
  post_fallback_tick_.Cancel();
  fallback_tick_fired_.Cancel();
  fallback_tick_pending_ = false;
}

void BrowserViewRenderer::PostFallbackTick() {
  DCHECK(fallback_tick_fired_.IsCancelled());
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::PostFallbackTick");
  post_fallback_tick_.Cancel();
  fallback_tick_fired_.Reset(base::Bind(&BrowserViewRenderer::FallbackTickFired,
                                        base::Unretained(this)));
  ui_task_runner_->PostDelayedTask(
      FROM_HERE, fallback_tick_fired_.callback(),
      base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
}

void BrowserViewRenderer::FallbackTickFired() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::FallbackTickFired");
  // This should only be called if OnDraw or DrawGL did not come in time, which
  // means fallback_tick_pending_ must still be true.
  DCHECK(fallback_tick_pending_);
  fallback_tick_fired_.Cancel();
  fallback_tick_pending_ = false;
  if (compositor_) {
    if (hardware_enabled_ && !size_.IsEmpty()) {
      CompositeHw();
    } else {
      ForceFakeCompositeSW();
    }
  }
}

void BrowserViewRenderer::ForceFakeCompositeSW() {
  DCHECK(compositor_);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  SkCanvas canvas(bitmap);
  CompositeSW(&canvas);
}

bool BrowserViewRenderer::CompositeSW(SkCanvas* canvas) {
  DCHECK(compositor_);
  CancelFallbackTick();
  ReturnResourceFromParent();
  return compositor_->DemandDrawSw(canvas);
}

void BrowserViewRenderer::UpdateCompositorIsActive() {
  if (compositor_) {
    compositor_->SetIsActive(!is_paused_ &&
                             (!attached_to_window_ || window_visible_));
  }
}

std::string BrowserViewRenderer::ToString() const {
  std::string str;
  base::StringAppendF(&str, "is_paused: %d ", is_paused_);
  base::StringAppendF(&str, "view_visible: %d ", view_visible_);
  base::StringAppendF(&str, "window_visible: %d ", window_visible_);
  base::StringAppendF(&str, "dip_scale: %f ", dip_scale_);
  base::StringAppendF(&str, "page_scale_factor: %f ", page_scale_factor_);
  base::StringAppendF(&str, "fallback_tick_pending: %d ",
                      fallback_tick_pending_);
  base::StringAppendF(&str, "view size: %s ", size_.ToString().c_str());
  base::StringAppendF(&str, "attached_to_window: %d ", attached_to_window_);
  base::StringAppendF(&str,
                      "global visible rect: %s ",
                      last_on_draw_global_visible_rect_.ToString().c_str());
  base::StringAppendF(
      &str, "scroll_offset_dip: %s ", scroll_offset_dip_.ToString().c_str());
  base::StringAppendF(&str,
                      "overscroll_rounding_error_: %s ",
                      overscroll_rounding_error_.ToString().c_str());
  base::StringAppendF(
      &str, "on_new_picture_enable: %d ", on_new_picture_enable_);
  base::StringAppendF(&str, "clear_view: %d ", clear_view_);
  return str;
}

}  // namespace android_webview
