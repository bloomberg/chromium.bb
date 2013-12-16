// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/desktop_capture_device_aura.h"

#include "base/logging.h"
#include "base/timer/timer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "content/browser/aura/image_transport_factory.h"
#include "content/browser/renderer_host/media/video_capture_device_impl.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"

namespace content {

namespace {

class DesktopVideoCaptureMachine
    : public VideoCaptureMachine,
      public aura::WindowObserver,
      public ui::CompositorObserver,
      public base::SupportsWeakPtr<DesktopVideoCaptureMachine> {
 public:
  DesktopVideoCaptureMachine(const DesktopMediaID& source);
  virtual ~DesktopVideoCaptureMachine();

  // VideoCaptureFrameSource overrides.
  virtual bool Start(
      const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy) OVERRIDE;
  virtual void Stop() OVERRIDE;

  // Implements aura::WindowObserver.
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // Implements ui::CompositorObserver.
  virtual void OnCompositingDidCommit(ui::Compositor* compositor) OVERRIDE {}
  virtual void OnCompositingStarted(ui::Compositor* compositor,
                                    base::TimeTicks start_time) OVERRIDE {}
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingAborted(ui::Compositor* compositor) OVERRIDE {}
  virtual void OnCompositingLockStateChanged(
      ui::Compositor* compositor) OVERRIDE {}
  virtual void OnUpdateVSyncParameters(ui::Compositor* compositor,
                                       base::TimeTicks timebase,
                                       base::TimeDelta interval) OVERRIDE {}

 private:
  // Captures a frame.
  // |dirty| is false for timer polls and true for compositor updates.
  void Capture(bool dirty);

  // Update capture size. Must be called on the UI thread.
  void UpdateCaptureSize();

  // Response callback for cc::Layer::RequestCopyOfOutput().
  void DidCopyOutput(
      scoped_refptr<media::VideoFrame> video_frame,
      base::Time start_time,
      const ThreadSafeCaptureOracle::CaptureFrameCallback& capture_frame_cb,
      scoped_ptr<cc::CopyOutputResult> result);

  // The window associated with the desktop.
  aura::Window* desktop_window_;

  // The layer associated with the desktop.
  ui::Layer* desktop_layer_;

  // The timer that kicks off period captures.
  base::Timer timer_;

  // The id of the window being captured.
  DesktopMediaID window_id_;

  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy_;

  // YUV readback pipeline.
  scoped_ptr<content::ReadbackYUVInterface> yuv_readback_pipeline_;

  DISALLOW_COPY_AND_ASSIGN(DesktopVideoCaptureMachine);
};

DesktopVideoCaptureMachine::DesktopVideoCaptureMachine(
    const DesktopMediaID& source)
    : desktop_window_(NULL),
      desktop_layer_(NULL),
      timer_(true, true),
      window_id_(source) {}

DesktopVideoCaptureMachine::~DesktopVideoCaptureMachine() {}

bool DesktopVideoCaptureMachine::Start(
    const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  desktop_window_ = content::DesktopMediaID::GetAuraWindowById(window_id_);
  if (!desktop_window_)
    return false;

  // If the desktop layer is already destroyed then return failure.
  desktop_layer_ = desktop_window_->layer();
  if (!desktop_layer_)
    return false;

  DCHECK(oracle_proxy.get());
  oracle_proxy_ = oracle_proxy;

  // Update capture size.
  UpdateCaptureSize();

  // Start observing window events.
  desktop_window_->AddObserver(this);

  // Start observing compositor updates.
  ui::Compositor* compositor = desktop_layer_->GetCompositor();
  if (!compositor)
    return false;

  compositor->AddObserver(this);

  // Starts timer.
  timer_.Start(FROM_HERE, oracle_proxy_->capture_period(),
               base::Bind(&DesktopVideoCaptureMachine::Capture, AsWeakPtr(),
                          false));

  started_ = true;
  return true;
}

void DesktopVideoCaptureMachine::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Stop observing window events.
  if (desktop_window_)
    desktop_window_->RemoveObserver(this);

  // Stop observing compositor updates.
  if (desktop_layer_) {
    ui::Compositor* compositor = desktop_layer_->GetCompositor();
    if (compositor)
      compositor->RemoveObserver(this);
  }

  // Stop timer.
  timer_.Stop();

  started_ = false;
}

void DesktopVideoCaptureMachine::UpdateCaptureSize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (oracle_proxy_ && desktop_layer_) {
    oracle_proxy_->UpdateCaptureSize(ui::ConvertSizeToPixel(
        desktop_layer_, desktop_layer_->bounds().size()));
  }
}

void DesktopVideoCaptureMachine::Capture(bool dirty) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do not capture if the desktop layer is already destroyed.
  if (!desktop_layer_)
    return;

  scoped_refptr<media::VideoFrame> frame;
  ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  const base::Time start_time = base::Time::Now();
  const VideoCaptureOracle::Event event =
      dirty ? VideoCaptureOracle::kCompositorUpdate
            : VideoCaptureOracle::kTimerPoll;
  if (oracle_proxy_->ObserveEventAndDecideCapture(
      event, start_time, &frame, &capture_frame_cb)) {
    scoped_ptr<cc::CopyOutputRequest> request =
        cc::CopyOutputRequest::CreateRequest(
            base::Bind(&DesktopVideoCaptureMachine::DidCopyOutput,
                       AsWeakPtr(), frame, start_time, capture_frame_cb));
    gfx::Rect window_rect =
        ui::ConvertRectToPixel(desktop_window_->layer(),
                               gfx::Rect(desktop_window_->bounds().width(),
                                         desktop_window_->bounds().height()));
    request->set_area(window_rect);
    desktop_layer_->RequestCopyOfOutput(request.Pass());
  }
}

static void CopyOutputFinishedForVideo(
    base::Time start_time,
    const ThreadSafeCaptureOracle::CaptureFrameCallback& capture_frame_cb,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  release_callback->Run(0, false);
  capture_frame_cb.Run(start_time, result);
}

void DesktopVideoCaptureMachine::DidCopyOutput(
    scoped_refptr<media::VideoFrame> video_frame,
    base::Time start_time,
    const ThreadSafeCaptureOracle::CaptureFrameCallback& capture_frame_cb,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty())
    return;

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's coded_size() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame =
      media::ComputeLetterboxRegion(gfx::Rect(video_frame->coded_size()),
                                    result->size());
  region_in_frame = gfx::Rect(region_in_frame.x() & ~1,
                              region_in_frame.y() & ~1,
                              region_in_frame.width() & ~1,
                              region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return;

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());
  if (!texture_mailbox.IsTexture())
    return;

  gfx::Rect result_rect(result->size());
  if (!yuv_readback_pipeline_ ||
      yuv_readback_pipeline_->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline_->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline_->scaler()->DstSize() != region_in_frame.size()) {
    yuv_readback_pipeline_.reset(
        gl_helper->CreateReadbackPipelineYUV(GLHelper::SCALER_QUALITY_FAST,
                                             result_rect.size(),
                                             result_rect,
                                             video_frame->coded_size(),
                                             region_in_frame,
                                             true,
                                             true));
  }
  yuv_readback_pipeline_->ReadbackYUV(
      texture_mailbox.name(), texture_mailbox.sync_point(), video_frame.get(),
      base::Bind(&CopyOutputFinishedForVideo, start_time, capture_frame_cb,
                 base::Passed(&release_callback)));
}

void DesktopVideoCaptureMachine::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  DCHECK(desktop_window_ && window == desktop_window_);

  // Post task to update capture size on UI thread.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &DesktopVideoCaptureMachine::UpdateCaptureSize, AsWeakPtr()));
}

void DesktopVideoCaptureMachine::OnWindowDestroyed(aura::Window* window) {
  DCHECK(desktop_window_ && window == desktop_window_);
  desktop_window_ = NULL;
  desktop_layer_ = NULL;

  // Post task to stop capture on UI thread.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &DesktopVideoCaptureMachine::Stop, AsWeakPtr()));
}

void DesktopVideoCaptureMachine::OnCompositingEnded(
    ui::Compositor* compositor) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &DesktopVideoCaptureMachine::Capture, AsWeakPtr(), true));
}

}  // namespace

DesktopCaptureDeviceAura::DesktopCaptureDeviceAura(
    const DesktopMediaID& source)
    : impl_(new VideoCaptureDeviceImpl(scoped_ptr<VideoCaptureMachine>(
        new DesktopVideoCaptureMachine(source)))) {}

DesktopCaptureDeviceAura::~DesktopCaptureDeviceAura() {
  DVLOG(2) << "DesktopCaptureDeviceAura@" << this << " destroying.";
}

// static
media::VideoCaptureDevice* DesktopCaptureDeviceAura::Create(
    const DesktopMediaID& source) {
  return new DesktopCaptureDeviceAura(source);
}

void DesktopCaptureDeviceAura::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  DVLOG(1) << "Allocating " << params.requested_format.frame_size.ToString();
  impl_->AllocateAndStart(params, client.Pass());
}

void DesktopCaptureDeviceAura::StopAndDeAllocate() {
  impl_->StopAndDeAllocate();
}

}  // namespace content
