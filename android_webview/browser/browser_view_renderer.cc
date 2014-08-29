// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"

#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/shared_renderer_state.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/android/jni_android.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "cc/output/compositor_frame.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/vector2d_conversions.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::SynchronousCompositorMemoryPolicy;

namespace android_webview {

namespace {

const int64 kFallbackTickTimeoutInMilliseconds = 100;

// Used to calculate memory allocation. Determined experimentally.
const size_t kMemoryMultiplier = 20;
const size_t kBytesPerPixel = 4;
const size_t kMemoryAllocationStep = 5 * 1024 * 1024;

// Used to calculate tile allocation. Determined experimentally.
const size_t kTileMultiplier = 12;
const size_t kTileAllocationStep = 20;
// This will be set by static function CalculateTileMemoryPolicy() during init.
// See AwMainDelegate::BasicStartupComplete.
size_t g_tile_area;

class TracedValue : public base::debug::ConvertableToTraceFormat {
 public:
  explicit TracedValue(base::Value* value) : value_(value) {}
  static scoped_refptr<base::debug::ConvertableToTraceFormat> FromValue(
      base::Value* value) {
    return scoped_refptr<base::debug::ConvertableToTraceFormat>(
        new TracedValue(value));
  }
  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE {
    std::string tmp;
    base::JSONWriter::Write(value_.get(), &tmp);
    *out += tmp;
  }

 private:
  virtual ~TracedValue() {}
  scoped_ptr<base::Value> value_;

  DISALLOW_COPY_AND_ASSIGN(TracedValue);
};

}  // namespace

// static
void BrowserViewRenderer::CalculateTileMemoryPolicy(bool use_zero_copy) {
  if (!use_zero_copy) {
    // Use chrome's default tile size, which varies from 256 to 512.
    // Be conservative here and use the smallest tile size possible.
    g_tile_area = 256 * 256;

    // Also use a high tile limit since there are no file descriptor issues.
    GlobalTileManager::GetInstance()->SetTileLimit(1000);
    return;
  }

  CommandLine* cl = CommandLine::ForCurrentProcess();
  const char kDefaultTileSize[] = "384";

  if (!cl->HasSwitch(switches::kDefaultTileWidth))
    cl->AppendSwitchASCII(switches::kDefaultTileWidth, kDefaultTileSize);

  if (!cl->HasSwitch(switches::kDefaultTileHeight))
    cl->AppendSwitchASCII(switches::kDefaultTileHeight, kDefaultTileSize);

  size_t tile_size;
  base::StringToSizeT(kDefaultTileSize, &tile_size);
  g_tile_area = tile_size * tile_size;
}

BrowserViewRenderer::BrowserViewRenderer(
    BrowserViewRendererClient* client,
    SharedRendererState* shared_renderer_state,
    content::WebContents* web_contents,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : client_(client),
      shared_renderer_state_(shared_renderer_state),
      web_contents_(web_contents),
      ui_task_runner_(ui_task_runner),
      compositor_(NULL),
      is_paused_(false),
      view_visible_(false),
      window_visible_(false),
      attached_to_window_(false),
      hardware_enabled_(false),
      dip_scale_(0.0),
      page_scale_factor_(1.0),
      on_new_picture_enable_(false),
      clear_view_(false),
      compositor_needs_continuous_invalidate_(false),
      block_invalidates_(false),
      width_(0),
      height_(0),
      num_tiles_(0u),
      num_bytes_(0u) {
  CHECK(web_contents_);
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, this);

  // Currently the logic in this class relies on |compositor_| remaining
  // NULL until the DidInitializeCompositor() call, hence it is not set here.
}

BrowserViewRenderer::~BrowserViewRenderer() {
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, NULL);
  // OnDetachedFromWindow should be called before the destructor, so the memory
  // policy should have already been updated.
}

// This function updates the cached memory policy in shared renderer state, as
// well as the tile resource allocation in GlobalTileManager.
void BrowserViewRenderer::TrimMemory(const int level, const bool visible) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Constants from Android ComponentCallbacks2.
  enum {
    TRIM_MEMORY_RUNNING_LOW = 10,
    TRIM_MEMORY_UI_HIDDEN = 20,
    TRIM_MEMORY_BACKGROUND = 40,
  };

  // Not urgent enough. TRIM_MEMORY_UI_HIDDEN is treated specially because
  // it does not indicate memory pressure, but merely that the app is
  // backgrounded.
  if (level < TRIM_MEMORY_RUNNING_LOW || level == TRIM_MEMORY_UI_HIDDEN)
    return;

  // Do not release resources on view we expect to get DrawGL soon.
  if (level < TRIM_MEMORY_BACKGROUND && visible)
    return;

  // Just set the memory limit to 0 and drop all tiles. This will be reset to
  // normal levels in the next DrawGL call.
  SynchronousCompositorMemoryPolicy zero_policy;
  if (memory_policy_ == zero_policy)
    return;

  TRACE_EVENT0("android_webview", "BrowserViewRenderer::TrimMemory");

  RequestMemoryPolicy(zero_policy);
  EnforceMemoryPolicyImmediately(zero_policy);
}

SynchronousCompositorMemoryPolicy
BrowserViewRenderer::CalculateDesiredMemoryPolicy() {
  SynchronousCompositorMemoryPolicy policy;
  size_t width = last_on_draw_global_visible_rect_.width();
  size_t height = last_on_draw_global_visible_rect_.height();
  policy.bytes_limit = kMemoryMultiplier * kBytesPerPixel * width * height;
  // Round up to a multiple of kMemoryAllocationStep.
  policy.bytes_limit =
      (policy.bytes_limit / kMemoryAllocationStep + 1) * kMemoryAllocationStep;

  size_t tiles = width * height * kTileMultiplier / g_tile_area;
  // Round up to a multiple of kTileAllocationStep. The minimum number of tiles
  // is also kTileAllocationStep.
  tiles = (tiles / kTileAllocationStep + 1) * kTileAllocationStep;
  policy.num_resources_limit = tiles;
  return policy;
}

// This function updates the cached memory policy in shared renderer state, as
// well as the tile resource allocation in GlobalTileManager.
void BrowserViewRenderer::RequestMemoryPolicy(
    SynchronousCompositorMemoryPolicy& new_policy) {
  // This will be used in SetNumTiles.
  num_bytes_ = new_policy.bytes_limit;

  GlobalTileManager* manager = GlobalTileManager::GetInstance();

  // The following line will call BrowserViewRenderer::SetTilesNum().
  manager->RequestTiles(new_policy.num_resources_limit, tile_manager_key_);
}

void BrowserViewRenderer::SetNumTiles(size_t num_tiles,
                                      bool effective_immediately) {
  if (num_tiles == num_tiles_)
    return;
  num_tiles_ = num_tiles;

  memory_policy_.num_resources_limit = num_tiles_;
  memory_policy_.bytes_limit = num_bytes_;

  if (effective_immediately)
    EnforceMemoryPolicyImmediately(memory_policy_);
}

void BrowserViewRenderer::EnforceMemoryPolicyImmediately(
    SynchronousCompositorMemoryPolicy new_policy) {
  compositor_->SetMemoryPolicy(new_policy);
  ForceFakeCompositeSW();
}

size_t BrowserViewRenderer::GetNumTiles() const {
  return memory_policy_.num_resources_limit;
}

bool BrowserViewRenderer::OnDraw(jobject java_canvas,
                                 bool is_hardware_canvas,
                                 const gfx::Vector2d& scroll,
                                 const gfx::Rect& global_visible_rect) {
  last_on_draw_scroll_offset_ = scroll;
  last_on_draw_global_visible_rect_ = global_visible_rect;

  if (clear_view_)
    return false;

  if (is_hardware_canvas && attached_to_window_ &&
      !switches::ForceAuxiliaryBitmap()) {
    return OnDrawHardware(java_canvas);
  }

  // Perform a software draw
  return OnDrawSoftware(java_canvas);
}

bool BrowserViewRenderer::OnDrawHardware(jobject java_canvas) {
  if (!compositor_)
    return false;

  if (!hardware_enabled_) {
    hardware_enabled_ = compositor_->InitializeHwDraw();
    if (hardware_enabled_) {
      tile_manager_key_ = GlobalTileManager::GetInstance()->PushBack(this);
      gpu::GLInProcessContext* share_context = compositor_->GetShareContext();
      DCHECK(share_context);
      shared_renderer_state_->SetSharedContext(share_context);
    }
  }
  if (!hardware_enabled_)
    return false;

  ReturnResourceFromParent();
  SynchronousCompositorMemoryPolicy new_policy = CalculateDesiredMemoryPolicy();
  RequestMemoryPolicy(new_policy);
  compositor_->SetMemoryPolicy(memory_policy_);

  scoped_ptr<DrawGLInput> draw_gl_input(new DrawGLInput);
  draw_gl_input->scroll_offset = last_on_draw_scroll_offset_;
  draw_gl_input->width = width_;
  draw_gl_input->height = height_;

  parent_draw_constraints_ = shared_renderer_state_->ParentDrawConstraints();
  gfx::Size surface_size(width_, height_);
  gfx::Rect viewport(surface_size);
  gfx::Rect clip = viewport;
  gfx::Transform transform_for_tile_priority =
      parent_draw_constraints_.transform;

  // If the WebView is on a layer, WebView does not know what transform is
  // applied onto the layer so global visible rect does not make sense here.
  // In this case, just use the surface rect for tiling.
  gfx::Rect viewport_rect_for_tile_priority;
  if (parent_draw_constraints_.is_layer)
    viewport_rect_for_tile_priority = parent_draw_constraints_.surface_rect;
  else
    viewport_rect_for_tile_priority = last_on_draw_global_visible_rect_;

  scoped_ptr<cc::CompositorFrame> frame =
      compositor_->DemandDrawHw(surface_size,
                                gfx::Transform(),
                                viewport,
                                clip,
                                viewport_rect_for_tile_priority,
                                transform_for_tile_priority);
  if (!frame.get())
    return false;

  GlobalTileManager::GetInstance()->DidUse(tile_manager_key_);

  frame->AssignTo(&draw_gl_input->frame);
  ReturnUnusedResource(shared_renderer_state_->PassDrawGLInput());
  shared_renderer_state_->SetDrawGLInput(draw_gl_input.Pass());
  DidComposite();
  return client_->RequestDrawGL(java_canvas, false);
}

void BrowserViewRenderer::UpdateParentDrawConstraints() {
  // Post an invalidate if the parent draw constraints are stale and there is
  // no pending invalidate.
  if (!parent_draw_constraints_.Equals(
          shared_renderer_state_->ParentDrawConstraints()))
    EnsureContinuousInvalidation(true);
}

void BrowserViewRenderer::ReturnUnusedResource(scoped_ptr<DrawGLInput> input) {
  if (!input.get())
    return;

  cc::CompositorFrameAck frame_ack;
  cc::TransferableResource::ReturnResources(
      input->frame.delegated_frame_data->resource_list,
      &frame_ack.resources);
  if (!frame_ack.resources.empty())
    compositor_->ReturnResources(frame_ack);
}

void BrowserViewRenderer::ReturnResourceFromParent() {
  cc::CompositorFrameAck frame_ack;
  shared_renderer_state_->SwapReturnedResources(&frame_ack.resources);
  if (!frame_ack.resources.empty()) {
    compositor_->ReturnResources(frame_ack);
  }
}

bool BrowserViewRenderer::OnDrawSoftware(jobject java_canvas) {
  if (!compositor_) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NoCompositor", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // TODO(hush): right now webview size is passed in as the auxiliary bitmap
  // size, which might hurt performace (only for software draws with auxiliary
  // bitmap). For better performance, get global visible rect, transform it
  // from screen space to view space, then intersect with the webview in
  // viewspace.  Use the resulting rect as the auxiliary
  // bitmap.
  return BrowserViewRendererJavaHelper::GetInstance()
      ->RenderViaAuxilaryBitmapIfNeeded(
          java_canvas,
          last_on_draw_scroll_offset_,
          gfx::Size(width_, height_),
          base::Bind(&BrowserViewRenderer::CompositeSW,
                     base::Unretained(this)));
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

  // Reset scroll back to the origin, will go back to the old
  // value when scroll_reset is out of scope.
  base::AutoReset<gfx::Vector2dF> scroll_reset(&scroll_offset_dip_,
                                               gfx::Vector2dF());

  SkPictureRecorder recorder;
  SkCanvas* rec_canvas = recorder.beginRecording(width, height, NULL, 0);
  if (compositor_)
    CompositeSW(rec_canvas);
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
  EnsureContinuousInvalidation(true);
}

void BrowserViewRenderer::SetIsPaused(bool paused) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetIsPaused",
                       TRACE_EVENT_SCOPE_THREAD,
                       "paused",
                       paused);
  is_paused_ = paused;
  EnsureContinuousInvalidation(false);
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
  EnsureContinuousInvalidation(false);
}

void BrowserViewRenderer::OnSizeChanged(int width, int height) {
  TRACE_EVENT_INSTANT2("android_webview",
                       "BrowserViewRenderer::OnSizeChanged",
                       TRACE_EVENT_SCOPE_THREAD,
                       "width",
                       width,
                       "height",
                       height);
  width_ = width;
  height_ = height;
}

void BrowserViewRenderer::OnAttachedToWindow(int width, int height) {
  TRACE_EVENT2("android_webview",
               "BrowserViewRenderer::OnAttachedToWindow",
               "width",
               width,
               "height",
               height);
  attached_to_window_ = true;
  width_ = width;
  height_ = height;
}

void BrowserViewRenderer::OnDetachedFromWindow() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDetachedFromWindow");
  attached_to_window_ = false;
  DCHECK(!hardware_enabled_);
}

void BrowserViewRenderer::ReleaseHardware() {
  DCHECK(hardware_enabled_);
  ReturnUnusedResource(shared_renderer_state_->PassDrawGLInput());
  ReturnResourceFromParent();
  DCHECK(shared_renderer_state_->ReturnedResourcesEmpty());

  compositor_->ReleaseHwDraw();
  shared_renderer_state_->SetSharedContext(NULL);
  hardware_enabled_ = false;

  SynchronousCompositorMemoryPolicy zero_policy;
  RequestMemoryPolicy(zero_policy);
  GlobalTileManager::GetInstance()->Remove(tile_manager_key_);
}

bool BrowserViewRenderer::IsVisible() const {
  // Ignore |window_visible_| if |attached_to_window_| is false.
  return view_visible_ && (!attached_to_window_ || window_visible_);
}

gfx::Rect BrowserViewRenderer::GetScreenRect() const {
  return gfx::Rect(client_->GetLocationOnScreen(), gfx::Size(width_, height_));
}

void BrowserViewRenderer::DidInitializeCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview",
               "BrowserViewRenderer::DidInitializeCompositor");
  DCHECK(compositor);
  DCHECK(!compositor_);
  compositor_ = compositor;
}

void BrowserViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::DidDestroyCompositor");
  DCHECK(compositor_);
  compositor_ = NULL;
  SynchronousCompositorMemoryPolicy zero_policy;
  DCHECK(memory_policy_ == zero_policy);
}

void BrowserViewRenderer::SetContinuousInvalidate(bool invalidate) {
  if (compositor_needs_continuous_invalidate_ == invalidate)
    return;

  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetContinuousInvalidate",
                       TRACE_EVENT_SCOPE_THREAD,
                       "invalidate",
                       invalidate);
  compositor_needs_continuous_invalidate_ = invalidate;

  EnsureContinuousInvalidation(false);
}

void BrowserViewRenderer::SetDipScale(float dip_scale) {
  dip_scale_ = dip_scale;
  CHECK(dip_scale_ > 0);
}

gfx::Vector2d BrowserViewRenderer::max_scroll_offset() const {
  DCHECK_GT(dip_scale_, 0);
  return gfx::ToCeiledVector2d(gfx::ScaleVector2d(
      max_scroll_offset_dip_, dip_scale_ * page_scale_factor_));
}

void BrowserViewRenderer::ScrollTo(gfx::Vector2d scroll_offset) {
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

  DCHECK_LE(0, scroll_offset_dip.x());
  DCHECK_LE(0, scroll_offset_dip.y());
  DCHECK_LE(scroll_offset_dip.x(), max_scroll_offset_dip_.x());
  DCHECK_LE(scroll_offset_dip.y(), max_scroll_offset_dip_.y());

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

  if (compositor_)
    compositor_->DidChangeRootLayerScrollOffset();
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
    gfx::Vector2dF scroll_offset_dip) {
  // TOOD(mkosiba): Add a DCHECK to say that this does _not_ get called during
  // DrawGl when http://crbug.com/249972 is fixed.
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

  DCHECK(0 <= scroll_offset.x());
  DCHECK(0 <= scroll_offset.y());
  DCHECK(scroll_offset.x() <= max_offset.x());
  DCHECK(scroll_offset.y() <= max_offset.y());

  client_->ScrollContainerViewTo(scroll_offset);
}

gfx::Vector2dF BrowserViewRenderer::GetTotalRootLayerScrollOffset() {
  return scroll_offset_dip_;
}

bool BrowserViewRenderer::IsExternalFlingActive() const {
  return client_->IsFlingActive();
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
      TracedValue::FromValue(
          RootLayerStateAsValue(total_scroll_offset_dip, scrollable_size_dip)
              .release()));

  DCHECK_GT(dip_scale_, 0);

  max_scroll_offset_dip_ = max_scroll_offset_dip;
  DCHECK_LE(0, max_scroll_offset_dip_.x());
  DCHECK_LE(0, max_scroll_offset_dip_.y());

  page_scale_factor_ = page_scale_factor;
  DCHECK_GT(page_scale_factor_, 0);

  client_->UpdateScrollState(max_scroll_offset(),
                             scrollable_size_dip,
                             page_scale_factor,
                             min_page_scale_factor,
                             max_page_scale_factor);
  SetTotalRootLayerScrollOffset(total_scroll_offset_dip);
}

scoped_ptr<base::Value> BrowserViewRenderer::RootLayerStateAsValue(
    const gfx::Vector2dF& total_scroll_offset_dip,
    const gfx::SizeF& scrollable_size_dip) {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetDouble("total_scroll_offset_dip.x", total_scroll_offset_dip.x());
  state->SetDouble("total_scroll_offset_dip.y", total_scroll_offset_dip.y());

  state->SetDouble("max_scroll_offset_dip.x", max_scroll_offset_dip_.x());
  state->SetDouble("max_scroll_offset_dip.y", max_scroll_offset_dip_.y());

  state->SetDouble("scrollable_size_dip.width", scrollable_size_dip.width());
  state->SetDouble("scrollable_size_dip.height", scrollable_size_dip.height());

  state->SetDouble("page_scale_factor", page_scale_factor_);
  return state.PassAs<base::Value>();
}

void BrowserViewRenderer::DidOverscroll(gfx::Vector2dF accumulated_overscroll,
                                        gfx::Vector2dF latest_overscroll_delta,
                                        gfx::Vector2dF current_fling_velocity) {
  const float physical_pixel_scale = dip_scale_ * page_scale_factor_;
  if (accumulated_overscroll == latest_overscroll_delta)
    overscroll_rounding_error_ = gfx::Vector2dF();
  gfx::Vector2dF scaled_overscroll_delta =
      gfx::ScaleVector2d(latest_overscroll_delta, physical_pixel_scale);
  gfx::Vector2d rounded_overscroll_delta = gfx::ToRoundedVector2d(
      scaled_overscroll_delta + overscroll_rounding_error_);
  overscroll_rounding_error_ =
      scaled_overscroll_delta - rounded_overscroll_delta;
  client_->DidOverscroll(rounded_overscroll_delta);
}

void BrowserViewRenderer::EnsureContinuousInvalidation(bool force_invalidate) {
  // This method should be called again when any of these conditions change.
  bool need_invalidate =
      compositor_needs_continuous_invalidate_ || force_invalidate;
  if (!need_invalidate || block_invalidates_)
    return;

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
  if (throttle_fallback_tick)
    return;

  block_invalidates_ = compositor_needs_continuous_invalidate_;

  // Unretained here is safe because the callbacks are cancelled when
  // they are destroyed.
  post_fallback_tick_.Reset(base::Bind(&BrowserViewRenderer::PostFallbackTick,
                                       base::Unretained(this)));
  fallback_tick_fired_.Cancel();

  // No need to reschedule fallback tick if compositor does not need to be
  // ticked. This can happen if this is reached because force_invalidate is
  // true.
  if (compositor_needs_continuous_invalidate_)
    ui_task_runner_->PostTask(FROM_HERE, post_fallback_tick_.callback());
}

void BrowserViewRenderer::PostFallbackTick() {
  DCHECK(fallback_tick_fired_.IsCancelled());
  fallback_tick_fired_.Reset(base::Bind(&BrowserViewRenderer::FallbackTickFired,
                                        base::Unretained(this)));
  if (compositor_needs_continuous_invalidate_) {
    ui_task_runner_->PostDelayedTask(
        FROM_HERE,
        fallback_tick_fired_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
  } else {
    // Pretend we just composited to unblock further invalidates.
    DidComposite();
  }
}

void BrowserViewRenderer::FallbackTickFired() {
  TRACE_EVENT1("android_webview",
               "BrowserViewRenderer::FallbackTickFired",
               "compositor_needs_continuous_invalidate_",
               compositor_needs_continuous_invalidate_);

  // This should only be called if OnDraw or DrawGL did not come in time, which
  // means block_invalidates_ must still be true.
  DCHECK(block_invalidates_);
  if (compositor_needs_continuous_invalidate_ && compositor_) {
    ForceFakeCompositeSW();
  } else {
    // Pretend we just composited to unblock further invalidates.
    DidComposite();
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
  ReturnResourceFromParent();
  bool result = compositor_->DemandDrawSw(canvas);
  DidComposite();
  return result;
}

void BrowserViewRenderer::DidComposite() {
  block_invalidates_ = false;
  post_fallback_tick_.Cancel();
  fallback_tick_fired_.Cancel();
  EnsureContinuousInvalidation(false);
}

std::string BrowserViewRenderer::ToString(AwDrawGLInfo* draw_info) const {
  std::string str;
  base::StringAppendF(&str, "is_paused: %d ", is_paused_);
  base::StringAppendF(&str, "view_visible: %d ", view_visible_);
  base::StringAppendF(&str, "window_visible: %d ", window_visible_);
  base::StringAppendF(&str, "dip_scale: %f ", dip_scale_);
  base::StringAppendF(&str, "page_scale_factor: %f ", page_scale_factor_);
  base::StringAppendF(&str,
                      "compositor_needs_continuous_invalidate: %d ",
                      compositor_needs_continuous_invalidate_);
  base::StringAppendF(&str, "block_invalidates: %d ", block_invalidates_);
  base::StringAppendF(&str, "view width height: [%d %d] ", width_, height_);
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
  if (draw_info) {
    base::StringAppendF(&str,
                        "clip left top right bottom: [%d %d %d %d] ",
                        draw_info->clip_left,
                        draw_info->clip_top,
                        draw_info->clip_right,
                        draw_info->clip_bottom);
    base::StringAppendF(&str,
                        "surface width height: [%d %d] ",
                        draw_info->width,
                        draw_info->height);
    base::StringAppendF(&str, "is_layer: %d ", draw_info->is_layer);
  }
  return str;
}

}  // namespace android_webview
