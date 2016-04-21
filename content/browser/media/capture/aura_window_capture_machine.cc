// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/aura_window_capture_machine.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
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
#include "ui/wm/public/activation_client.h"

namespace content {

AuraWindowCaptureMachine::AuraWindowCaptureMachine()
    : desktop_window_(NULL),
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

  DCHECK(oracle_proxy);
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
    cursor_renderer_.reset();
  }

  callback.Run();
}

void AuraWindowCaptureMachine::MaybeCaptureForRefresh() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AuraWindowCaptureMachine::Capture,
                 // Use of Unretained() is safe here since this task must run
                 // before InternalStop().
                 base::Unretained(this),
                 false));
}

void AuraWindowCaptureMachine::SetWindow(aura::Window* window) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(!desktop_window_);
  desktop_window_ = window;
  cursor_renderer_.reset(new CursorRendererAura(window, kCursorAlwaysEnabled));

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
  if (oracle_proxy_ && desktop_window_) {
     ui::Layer* layer = desktop_window_->layer();
     oracle_proxy_->UpdateCaptureSize(ui::ConvertSizeToPixel(
         layer, layer->bounds().size()));
  }
  cursor_renderer_->Clear();
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
            : media::VideoCaptureOracle::kActiveRefreshRequest;
  if (oracle_proxy_->ObserveEventAndDecideCapture(
          event, gfx::Rect(), start_time, &frame, &capture_frame_cb)) {
    std::unique_ptr<cc::CopyOutputRequest> request =
        cc::CopyOutputRequest::CreateRequest(base::Bind(
            &AuraWindowCaptureMachine::DidCopyOutput,
            weak_factory_.GetWeakPtr(), frame, start_time, capture_frame_cb));
    gfx::Rect window_rect = gfx::Rect(desktop_window_->bounds().width(),
                                      desktop_window_->bounds().height());
    request->set_area(window_rect);
    desktop_window_->layer()->RequestCopyOfOutput(std::move(request));
  }
}

void AuraWindowCaptureMachine::DidCopyOutput(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks start_time,
    const CaptureFrameCallback& capture_frame_cb,
    std::unique_ptr<cc::CopyOutputResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static bool first_call = true;

  bool succeeded = ProcessCopyOutputResponse(
      video_frame, start_time, capture_frame_cb, std::move(result));

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
    std::unique_ptr<cc::CopyOutputResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result->IsEmpty() || result->size().IsEmpty() || !desktop_window_)
    return false;
  DCHECK(video_frame);

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
  display_compositor::GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return false;

  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return false;

  gfx::Rect result_rect(result->size());
  if (!yuv_readback_pipeline_ ||
      yuv_readback_pipeline_->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline_->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline_->scaler()->DstSize() != region_in_frame.size()) {
    yuv_readback_pipeline_.reset(gl_helper->CreateReadbackPipelineYUV(
        display_compositor::GLHelper::SCALER_QUALITY_FAST, result_rect.size(),
        result_rect, region_in_frame.size(), true, true));
  }

  cursor_renderer_->SnapshotCursorState(region_in_frame);
  yuv_readback_pipeline_->ReadbackYUV(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(),
      video_frame->visible_rect(),
      video_frame->stride(media::VideoFrame::kYPlane),
      video_frame->data(media::VideoFrame::kYPlane),
      video_frame->stride(media::VideoFrame::kUPlane),
      video_frame->data(media::VideoFrame::kUPlane),
      video_frame->stride(media::VideoFrame::kVPlane),
      video_frame->data(media::VideoFrame::kVPlane), region_in_frame.origin(),
      base::Bind(&CopyOutputFinishedForVideo, weak_factory_.GetWeakPtr(),
                 start_time, capture_frame_cb, video_frame,
                 base::Passed(&release_callback)));
  media::LetterboxYUV(video_frame.get(), region_in_frame);
  return true;
}

using CaptureFrameCallback =
    media::ThreadSafeCaptureOracle::CaptureFrameCallback;

void AuraWindowCaptureMachine::CopyOutputFinishedForVideo(
    base::WeakPtr<AuraWindowCaptureMachine> machine,
    base::TimeTicks start_time,
    const CaptureFrameCallback& capture_frame_cb,
    const scoped_refptr<media::VideoFrame>& target,
    std::unique_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  release_callback->Run(gpu::SyncToken(), false);

  // Render the cursor and deliver the captured frame if the
  // AuraWindowCaptureMachine has not been stopped (i.e., the WeakPtr is
  // still valid).
  if (machine) {
    if (machine->cursor_renderer_ && result)
      machine->cursor_renderer_->RenderOnVideoFrame(target);
    capture_frame_cb.Run(target, start_time, result);
  }
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

  oracle_proxy_->ReportError(FROM_HERE, "OnWindowDestroying()");
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
