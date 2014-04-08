// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"

#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/shared_renderer_state.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/android/jni_android.h"
#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/vector2d_conversions.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {

const int64 kFallbackTickTimeoutInMilliseconds = 20;

class AutoResetWithLock {
 public:
  AutoResetWithLock(gfx::Vector2dF* scoped_variable,
                    gfx::Vector2dF new_value,
                    base::Lock& lock)
      : scoped_variable_(scoped_variable),
        original_value_(*scoped_variable),
        lock_(lock) {
    base::AutoLock auto_lock(lock_);
    *scoped_variable_ = new_value;
  }

  ~AutoResetWithLock() {
    base::AutoLock auto_lock(lock_);
    *scoped_variable_ = original_value_;
  }

 private:
  gfx::Vector2dF* scoped_variable_;
  gfx::Vector2dF original_value_;
  base::Lock& lock_;

  DISALLOW_COPY_AND_ASSIGN(AutoResetWithLock);
};

}  // namespace

BrowserViewRenderer::BrowserViewRenderer(
    BrowserViewRendererClient* client,
    SharedRendererState* shared_renderer_state,
    content::WebContents* web_contents,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : client_(client),
      shared_renderer_state_(shared_renderer_state),
      web_contents_(web_contents),
      weak_factory_on_ui_thread_(this),
      ui_thread_weak_ptr_(weak_factory_on_ui_thread_.GetWeakPtr()),
      ui_task_runner_(ui_task_runner),
      has_compositor_(false),
      is_paused_(false),
      view_visible_(false),
      window_visible_(false),
      attached_to_window_(false),
      dip_scale_(0.0),
      page_scale_factor_(1.0),
      on_new_picture_enable_(false),
      clear_view_(false),
      compositor_needs_continuous_invalidate_(false),
      block_invalidates_(false),
      width_(0),
      height_(0) {
  CHECK(web_contents_);
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, this);

  // Currently the logic in this class relies on |has_compositor_| remaining
  // false until the DidInitializeCompositor() call, hence it is not set here.
}

BrowserViewRenderer::~BrowserViewRenderer() {
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, NULL);
}

bool BrowserViewRenderer::OnDraw(jobject java_canvas,
                                 bool is_hardware_canvas,
                                 const gfx::Vector2d& scroll,
                                 const gfx::Rect& global_visible_rect,
                                 const gfx::Rect& clip) {
  draw_gl_input_.frame_id++;
  draw_gl_input_.scroll_offset = scroll;
  draw_gl_input_.global_visible_rect = global_visible_rect;
  draw_gl_input_.width = width_;
  draw_gl_input_.height = height_;
  if (clear_view_)
    return false;
  if (is_hardware_canvas && attached_to_window_) {
    shared_renderer_state_->SetDrawGLInput(draw_gl_input_);
    // We should be performing a hardware draw here. If we don't have the
    // compositor yet or if RequestDrawGL fails, it means we failed this draw
    // and thus return false here to clear to background color for this draw.
    return has_compositor_ && client_->RequestDrawGL(java_canvas);
  }
  // Perform a software draw
  return DrawSWInternal(java_canvas, clip);
}

void BrowserViewRenderer::DidDrawGL(const DrawGLResult& result) {
  DidComposite(!result.clip_contains_visible_rect);
}

bool BrowserViewRenderer::DrawSWInternal(jobject java_canvas,
                                         const gfx::Rect& clip) {
  if (clip.IsEmpty()) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_EmptyClip", TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  if (!has_compositor_) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NoCompositor", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return BrowserViewRendererJavaHelper::GetInstance()
      ->RenderViaAuxilaryBitmapIfNeeded(
            java_canvas,
            draw_gl_input_.scroll_offset,
            clip,
            base::Bind(&BrowserViewRenderer::CompositeSW,
                       base::Unretained(this)));
}

skia::RefPtr<SkPicture> BrowserViewRenderer::CapturePicture(int width,
                                                            int height) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::CapturePicture");

  // Return empty Picture objects for empty SkPictures.
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  if (width <= 0 || height <= 0) {
    return picture;
  }

  // Reset scroll back to the origin, will go back to the old
  // value when scroll_reset is out of scope.
  AutoResetWithLock scroll_reset(
      &scroll_offset_dip_, gfx::Vector2dF(), scroll_offset_dip_lock_);

  SkCanvas* rec_canvas = picture->beginRecording(width, height, 0);
  if (has_compositor_)
    CompositeSW(rec_canvas);
  picture->endRecording();
  return picture;
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
}

bool BrowserViewRenderer::IsAttachedToWindow() const {
  return attached_to_window_;
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
  DCHECK(!has_compositor_);
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  has_compositor_ = true;
  shared_renderer_state_->SetCompositorOnUiThread(compositor);
}

void BrowserViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::DidDestroyCompositor");
  DCHECK(has_compositor_);
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  has_compositor_ = false;
  shared_renderer_state_->SetCompositorOnUiThread(NULL);
}

void BrowserViewRenderer::SetContinuousInvalidate(bool invalidate) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserViewRenderer::SetContinuousInvalidate,
                   ui_thread_weak_ptr_,
                   invalidate));
    return;
  }
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

  {
    base::AutoLock lock(scroll_offset_dip_lock_);
    if (scroll_offset_dip_ == scroll_offset_dip)
      return;

    scroll_offset_dip_ = scroll_offset_dip;
  }

  if (has_compositor_)
    shared_renderer_state_->GetCompositor()->
        DidChangeRootLayerScrollOffset();
}

void BrowserViewRenderer::DidUpdateContent() {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&BrowserViewRenderer::DidUpdateContent,
                                         ui_thread_weak_ptr_));
    return;
  }
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidUpdateContent",
                       TRACE_EVENT_SCOPE_THREAD);
  clear_view_ = false;
  if (on_new_picture_enable_)
    client_->OnNewPicture();
}

void BrowserViewRenderer::SetMaxRootLayerScrollOffset(
    gfx::Vector2dF new_value_dip) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserViewRenderer::SetMaxRootLayerScrollOffset,
                   ui_thread_weak_ptr_,
                   new_value_dip));
    return;
  }
  DCHECK_GT(dip_scale_, 0);

  max_scroll_offset_dip_ = new_value_dip;
  DCHECK_LE(0, max_scroll_offset_dip_.x());
  DCHECK_LE(0, max_scroll_offset_dip_.y());

  client_->SetMaxContainerViewScrollOffset(max_scroll_offset());
}

void BrowserViewRenderer::SetTotalRootLayerScrollOffset(
    gfx::Vector2dF scroll_offset_dip) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserViewRenderer::SetTotalRootLayerScrollOffset,
                   ui_thread_weak_ptr_,
                   scroll_offset_dip));
    return;
  }

  {
    base::AutoLock lock(scroll_offset_dip_lock_);
    // TOOD(mkosiba): Add a DCHECK to say that this does _not_ get called during
    // DrawGl when http://crbug.com/249972 is fixed.
    if (scroll_offset_dip_ == scroll_offset_dip)
      return;

    scroll_offset_dip_ = scroll_offset_dip;
  }

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
  // Disabled because the conditions are being violated while running
  // AwZoomTest.testMagnification, see http://crbug.com/340648
  // DCHECK(scroll_offset.x() <= max_offset.x());
  // DCHECK(scroll_offset.y() <= max_offset.y());

  client_->ScrollContainerViewTo(scroll_offset);
}

gfx::Vector2dF BrowserViewRenderer::GetTotalRootLayerScrollOffset() {
  base::AutoLock lock(scroll_offset_dip_lock_);
  return scroll_offset_dip_;
}

bool BrowserViewRenderer::IsExternalFlingActive() const {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    // TODO(boliu): This is short term hack since we cannot call into
    // view system on non-UI thread.
    return false;
  }
  return client_->IsFlingActive();
}

void BrowserViewRenderer::SetRootLayerPageScaleFactorAndLimits(
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserViewRenderer::SetRootLayerPageScaleFactorAndLimits,
                   ui_thread_weak_ptr_,
                   page_scale_factor,
                   min_page_scale_factor,
                   max_page_scale_factor));
    return;
  }
  page_scale_factor_ = page_scale_factor;
  DCHECK_GT(page_scale_factor_, 0);
  client_->SetPageScaleFactorAndLimits(
      page_scale_factor, min_page_scale_factor, max_page_scale_factor);
  client_->SetMaxContainerViewScrollOffset(max_scroll_offset());
}

void BrowserViewRenderer::SetRootLayerScrollableSize(
    gfx::SizeF scrollable_size) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserViewRenderer::SetRootLayerScrollableSize,
                   ui_thread_weak_ptr_,
                   scrollable_size));
    return;
  }
  client_->SetContentsSize(scrollable_size);
}

void BrowserViewRenderer::DidOverscroll(gfx::Vector2dF accumulated_overscroll,
                                        gfx::Vector2dF latest_overscroll_delta,
                                        gfx::Vector2dF current_fling_velocity) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserViewRenderer::DidOverscroll,
                   ui_thread_weak_ptr_,
                   accumulated_overscroll,
                   latest_overscroll_delta,
                   current_fling_velocity));
    return;
  }
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

  // Unretained here is safe because the callback is cancelled when
  // |fallback_tick_| is destroyed.
  fallback_tick_.Reset(base::Bind(&BrowserViewRenderer::FallbackTickFired,
                                  base::Unretained(this)));

  // No need to reschedule fallback tick if compositor does not need to be
  // ticked. This can happen if this is reached because force_invalidate is
  // true.
  if (compositor_needs_continuous_invalidate_) {
    ui_task_runner_->PostDelayedTask(
        FROM_HERE,
        fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
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
  if (compositor_needs_continuous_invalidate_ && has_compositor_)
    ForceFakeCompositeSW();
}

void BrowserViewRenderer::ForceFakeCompositeSW() {
  DCHECK(has_compositor_);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  SkCanvas canvas(bitmap);
  CompositeSW(&canvas);
}

bool BrowserViewRenderer::CompositeSW(SkCanvas* canvas) {
  DCHECK(has_compositor_);
  bool result = shared_renderer_state_->GetCompositor()->
      DemandDrawSw(canvas);
  DidComposite(false);
  return result;
}

void BrowserViewRenderer::DidComposite(bool force_invalidate) {
  fallback_tick_.Cancel();
  block_invalidates_ = false;
  EnsureContinuousInvalidation(force_invalidate);
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
                      draw_gl_input_.global_visible_rect.ToString().c_str());
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
