// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/aura_window_capture_machine.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/timer/timer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/power_save_blocker.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_util.h"
#include "media/capture/content/thread_safe_capture_oracle.h"
#include "media/capture/content/video_capture_oracle.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"

namespace content {

namespace {

int clip_byte(int x) {
  return std::max(0, std::min(x, 255));
}

int alpha_blend(int alpha, int src, int dst) {
  return (src * alpha + dst * (255 - alpha)) / 255;
}

// Helper function to composite a cursor bitmap on a YUV420 video frame.
void RenderCursorOnVideoFrame(
    const scoped_refptr<media::VideoFrame>& target,
    const SkBitmap& cursor_bitmap,
    const gfx::Point& cursor_position) {
  DCHECK(target.get());
  DCHECK(!cursor_bitmap.isNull());

  gfx::Rect rect = gfx::IntersectRects(
      gfx::Rect(cursor_bitmap.width(), cursor_bitmap.height()) +
          gfx::Vector2d(cursor_position.x(), cursor_position.y()),
      target->visible_rect());

  cursor_bitmap.lockPixels();
  for (int y = rect.y(); y < rect.bottom(); ++y) {
    int cursor_y = y - cursor_position.y();
    uint8* yplane = target->data(media::VideoFrame::kYPlane) +
        y * target->row_bytes(media::VideoFrame::kYPlane);
    uint8* uplane = target->data(media::VideoFrame::kUPlane) +
        (y / 2) * target->row_bytes(media::VideoFrame::kUPlane);
    uint8* vplane = target->data(media::VideoFrame::kVPlane) +
        (y / 2) * target->row_bytes(media::VideoFrame::kVPlane);
    for (int x = rect.x(); x < rect.right(); ++x) {
      int cursor_x = x - cursor_position.x();
      SkColor color = cursor_bitmap.getColor(cursor_x, cursor_y);
      int alpha = SkColorGetA(color);
      int color_r = SkColorGetR(color);
      int color_g = SkColorGetG(color);
      int color_b = SkColorGetB(color);
      int color_y = clip_byte(((color_r * 66 + color_g * 129 + color_b * 25 +
                                128) >> 8) + 16);
      yplane[x] = alpha_blend(alpha, color_y, yplane[x]);

      // Only sample U and V at even coordinates.
      if ((x % 2 == 0) && (y % 2 == 0)) {
        int color_u = clip_byte(((color_r * -38 + color_g * -74 +
                                  color_b * 112 + 128) >> 8) + 128);
        int color_v = clip_byte(((color_r * 112 + color_g * -94 +
                                  color_b * -18 + 128) >> 8) + 128);
        uplane[x / 2] = alpha_blend(alpha, color_u, uplane[x / 2]);
        vplane[x / 2] = alpha_blend(alpha, color_v, vplane[x / 2]);
      }
    }
  }
  cursor_bitmap.unlockPixels();
}

using CaptureFrameCallback =
    media::ThreadSafeCaptureOracle::CaptureFrameCallback;

void CopyOutputFinishedForVideo(
    base::WeakPtr<AuraWindowCaptureMachine> machine,
    base::TimeTicks start_time,
    const CaptureFrameCallback& capture_frame_cb,
    const scoped_refptr<media::VideoFrame>& target,
    const SkBitmap& cursor_bitmap,
    const gfx::Point& cursor_position,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!cursor_bitmap.isNull())
    RenderCursorOnVideoFrame(target, cursor_bitmap, cursor_position);
  release_callback->Run(0, false);

  // Only deliver the captured frame if the AuraWindowCaptureMachine has not
  // been stopped (i.e., the WeakPtr is still valid).
  if (machine.get())
    capture_frame_cb.Run(target, start_time, result);
}

void RunSingleReleaseCallback(scoped_ptr<cc::SingleReleaseCallback> cb,
                              uint32 sync_point) {
  cb->Run(sync_point, false);
}

}  // namespace

AuraWindowCaptureMachine::AuraWindowCaptureMachine()
    : desktop_window_(NULL),
      timer_(true, true),
      screen_capture_(false),
      weak_factory_(this) {}

AuraWindowCaptureMachine::~AuraWindowCaptureMachine() {}

void AuraWindowCaptureMachine::Start(
  const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
  const media::VideoCaptureParams& params,
  const base::Callback<void(bool)> callback) {
  // Starts the capture machine asynchronously.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AuraWindowCaptureMachine::InternalStart,
                 base::Unretained(this),
                 oracle_proxy,
                 params),
      callback);
}

bool AuraWindowCaptureMachine::InternalStart(
    const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The window might be destroyed between SetWindow() and Start().
  if (!desktop_window_)
    return false;

  // If the associated layer is already destroyed then return failure.
  ui::Layer* layer = desktop_window_->layer();
  if (!layer)
    return false;

  DCHECK(oracle_proxy.get());
  oracle_proxy_ = oracle_proxy;
  capture_params_ = params;

  // Update capture size.
  UpdateCaptureSize();

  // Start observing compositor updates.
  if (desktop_window_->GetHost())
    desktop_window_->GetHost()->compositor()->AddObserver(this);

  power_save_blocker_.reset(
      PowerSaveBlocker::Create(
          PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
          PowerSaveBlocker::kReasonOther,
          "DesktopCaptureDevice is running").release());

  // Starts timer.
  timer_.Start(FROM_HERE,
               std::max(oracle_proxy_->min_capture_period(),
                        base::TimeDelta::FromMilliseconds(media::
                            VideoCaptureOracle::kMinTimerPollPeriodMillis)),
               base::Bind(&AuraWindowCaptureMachine::Capture,
                          base::Unretained(this), false));

  return true;
}

void AuraWindowCaptureMachine::Stop(const base::Closure& callback) {
  // Stops the capture machine asynchronously.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &AuraWindowCaptureMachine::InternalStop,
          base::Unretained(this),
          callback));
}

void AuraWindowCaptureMachine::InternalStop(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Cancel any and all outstanding callbacks owned by external modules.
  weak_factory_.InvalidateWeakPtrs();

  power_save_blocker_.reset();

  // Stop observing compositor and window events.
  if (desktop_window_) {
    aura::WindowTreeHost* window_host = desktop_window_->GetHost();
    // In the host destructor the compositor is destroyed before the window.
    if (window_host && window_host->compositor())
      window_host->compositor()->RemoveObserver(this);
    desktop_window_->RemoveObserver(this);
    desktop_window_ = NULL;
  }

  // Stop timer.
  timer_.Stop();

  callback.Run();
}

void AuraWindowCaptureMachine::SetWindow(aura::Window* window) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(!desktop_window_);
  desktop_window_ = window;

  // Start observing window events.
  desktop_window_->AddObserver(this);

  // We must store this for the UMA reporting in DidCopyOutput() as
  // desktop_window_ might be destroyed at that point.
  screen_capture_ = window->IsRootWindow();
  IncrementDesktopCaptureCounter(screen_capture_ ? SCREEN_CAPTURER_CREATED
                                                 : WINDOW_CAPTURER_CREATED);
}

void AuraWindowCaptureMachine::UpdateCaptureSize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (oracle_proxy_.get() && desktop_window_) {
     ui::Layer* layer = desktop_window_->layer();
     oracle_proxy_->UpdateCaptureSize(ui::ConvertSizeToPixel(
         layer, layer->bounds().size()));
  }
  ClearCursorState();
}

void AuraWindowCaptureMachine::Capture(bool dirty) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Do not capture if the desktop window is already destroyed.
  if (!desktop_window_)
    return;

  scoped_refptr<media::VideoFrame> frame;
  media::ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  // TODO(miu): Need to fix this so the compositor is providing the presentation
  // timestamps and damage regions, to leverage the frame timestamp rewriting
  // logic.  http://crbug.com/492839
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const media::VideoCaptureOracle::Event event =
      dirty ? media::VideoCaptureOracle::kCompositorUpdate
            : media::VideoCaptureOracle::kTimerPoll;
  if (oracle_proxy_->ObserveEventAndDecideCapture(
          event, gfx::Rect(), start_time, &frame, &capture_frame_cb)) {
    scoped_ptr<cc::CopyOutputRequest> request =
        cc::CopyOutputRequest::CreateRequest(
            base::Bind(&AuraWindowCaptureMachine::DidCopyOutput,
                       weak_factory_.GetWeakPtr(),
                       frame, start_time, capture_frame_cb));
    gfx::Rect window_rect = gfx::Rect(desktop_window_->bounds().width(),
                                      desktop_window_->bounds().height());
    request->set_area(window_rect);
    desktop_window_->layer()->RequestCopyOfOutput(request.Pass());
  }
}

void AuraWindowCaptureMachine::DidCopyOutput(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks start_time,
    const CaptureFrameCallback& capture_frame_cb,
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static bool first_call = true;

  bool succeeded = ProcessCopyOutputResponse(
      video_frame, start_time, capture_frame_cb, result.Pass());

  base::TimeDelta capture_time = base::TimeTicks::Now() - start_time;

  // The two UMA_ blocks must be put in its own scope since it creates a static
  // variable which expected constant histogram name.
  if (screen_capture_) {
    UMA_HISTOGRAM_TIMES(kUmaScreenCaptureTime, capture_time);
  } else {
    UMA_HISTOGRAM_TIMES(kUmaWindowCaptureTime, capture_time);
  }

  if (first_call) {
    first_call = false;
    if (screen_capture_) {
      IncrementDesktopCaptureCounter(succeeded ? FIRST_SCREEN_CAPTURE_SUCCEEDED
                                               : FIRST_SCREEN_CAPTURE_FAILED);
    } else {
      IncrementDesktopCaptureCounter(succeeded
                                         ? FIRST_WINDOW_CAPTURE_SUCCEEDED
                                         : FIRST_WINDOW_CAPTURE_FAILED);
    }
  }
}

bool AuraWindowCaptureMachine::ProcessCopyOutputResponse(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks start_time,
    const CaptureFrameCallback& capture_frame_cb,
    scoped_ptr<cc::CopyOutputResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result->IsEmpty() || result->size().IsEmpty() || !desktop_window_)
    return false;

  if (capture_params_.requested_format.pixel_storage ==
      media::PIXEL_STORAGE_TEXTURE) {
    DCHECK_EQ(media::PIXEL_FORMAT_ARGB,
              capture_params_.requested_format.pixel_format);
    DCHECK(!video_frame.get());
    cc::TextureMailbox texture_mailbox;
    scoped_ptr<cc::SingleReleaseCallback> release_callback;
    result->TakeTexture(&texture_mailbox, &release_callback);
    DCHECK(texture_mailbox.IsTexture());
    if (!texture_mailbox.IsTexture())
      return false;
    video_frame = media::VideoFrame::WrapNativeTexture(
        media::PIXEL_FORMAT_ARGB,
        gpu::MailboxHolder(texture_mailbox.mailbox(), texture_mailbox.target(),
                           texture_mailbox.sync_point()),
        base::Bind(&RunSingleReleaseCallback, base::Passed(&release_callback)),
        result->size(), gfx::Rect(result->size()), result->size(),
        base::TimeDelta());
    capture_frame_cb.Run(video_frame, start_time, true);
    return true;
  } else {
    DCHECK(video_frame.get());
  }

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's visible_rect() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
      video_frame->visible_rect(), result->size());
  region_in_frame = gfx::Rect(region_in_frame.x() & ~1,
                              region_in_frame.y() & ~1,
                              region_in_frame.width() & ~1,
                              region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return false;

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return false;

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return false;

  gfx::Rect result_rect(result->size());
  if (!yuv_readback_pipeline_ ||
      yuv_readback_pipeline_->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline_->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline_->scaler()->DstSize() != region_in_frame.size()) {
    yuv_readback_pipeline_.reset(
        gl_helper->CreateReadbackPipelineYUV(GLHelper::SCALER_QUALITY_FAST,
                                             result_rect.size(),
                                             result_rect,
                                             region_in_frame.size(),
                                             true,
                                             true));
  }

  gfx::Point cursor_position_in_frame = UpdateCursorState(region_in_frame);
  yuv_readback_pipeline_->ReadbackYUV(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      video_frame.get(),
      region_in_frame.origin(),
      base::Bind(&CopyOutputFinishedForVideo,
                 weak_factory_.GetWeakPtr(),
                 start_time,
                 capture_frame_cb,
                 video_frame,
                 scaled_cursor_bitmap_,
                 cursor_position_in_frame,
                 base::Passed(&release_callback)));
  return true;
}

gfx::Point AuraWindowCaptureMachine::UpdateCursorState(
    const gfx::Rect& region_in_frame) {
  const gfx::Rect window_bounds = desktop_window_->GetBoundsInScreen();
  gfx::Point cursor_position = aura::Env::GetInstance()->last_mouse_location();
  if (!window_bounds.Contains(cursor_position)) {
    // Return early if there is no need to draw the cursor.
    ClearCursorState();
    return gfx::Point();
  }

  gfx::NativeCursor cursor = desktop_window_->GetHost()->last_cursor();
  gfx::Point cursor_hot_point;
  if (last_cursor_ != cursor ||
      window_size_when_cursor_last_updated_ != window_bounds.size()) {
    SkBitmap cursor_bitmap;
    if (ui::GetCursorBitmap(cursor, &cursor_bitmap, &cursor_hot_point)) {
      const int scaled_width = cursor_bitmap.width() *
          region_in_frame.width() / window_bounds.width();
      const int scaled_height = cursor_bitmap.height() *
          region_in_frame.height() / window_bounds.height();
      if (scaled_width <= 0 || scaled_height <= 0) {
        ClearCursorState();
        return gfx::Point();
      }
      scaled_cursor_bitmap_ = skia::ImageOperations::Resize(
          cursor_bitmap,
          skia::ImageOperations::RESIZE_BEST,
          scaled_width,
          scaled_height);
      last_cursor_ = cursor;
      window_size_when_cursor_last_updated_ = window_bounds.size();
    } else {
      // Clear cursor state if ui::GetCursorBitmap failed so that we do not
      // render cursor on the captured frame.
      ClearCursorState();
    }
  }

  cursor_position.Offset(-window_bounds.x() - cursor_hot_point.x(),
                         -window_bounds.y() - cursor_hot_point.y());
  return gfx::Point(
      region_in_frame.x() + cursor_position.x() *
          region_in_frame.width() / window_bounds.width(),
      region_in_frame.y() + cursor_position.y() *
          region_in_frame.height() / window_bounds.height());
}

void AuraWindowCaptureMachine::ClearCursorState() {
  last_cursor_ = ui::Cursor();
  window_size_when_cursor_last_updated_ = gfx::Size();
  scaled_cursor_bitmap_.reset();
}

void AuraWindowCaptureMachine::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(desktop_window_ && window == desktop_window_);

  // Post a task to update capture size after first returning to the event loop.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &AuraWindowCaptureMachine::UpdateCaptureSize,
      weak_factory_.GetWeakPtr()));
}

void AuraWindowCaptureMachine::OnWindowDestroying(aura::Window* window) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  InternalStop(base::Bind(&base::DoNothing));

  oracle_proxy_->ReportError("OnWindowDestroying()");
}

void AuraWindowCaptureMachine::OnWindowAddedToRootWindow(
    aura::Window* window) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(window == desktop_window_);

  window->GetHost()->compositor()->AddObserver(this);
}

void AuraWindowCaptureMachine::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(window == desktop_window_);

  window->GetHost()->compositor()->RemoveObserver(this);
}

void AuraWindowCaptureMachine::OnCompositingEnded(
    ui::Compositor* compositor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(miu): The CopyOutputRequest should be made earlier, at WillCommit().
  // http://crbug.com/492839
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AuraWindowCaptureMachine::Capture, weak_factory_.GetWeakPtr(),
                 true));
}

}  // namespace content
