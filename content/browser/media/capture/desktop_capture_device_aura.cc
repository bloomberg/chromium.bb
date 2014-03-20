// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device_aura.h"

#include "base/logging.h"
#include "base/timer/timer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/media/capture/content_video_capture_device_core.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
  DCHECK(target);
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
  virtual void Stop(const base::Closure& callback) OVERRIDE;

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

 private:
  // Captures a frame.
  // |dirty| is false for timer polls and true for compositor updates.
  void Capture(bool dirty);

  // Update capture size. Must be called on the UI thread.
  void UpdateCaptureSize();

  // Response callback for cc::Layer::RequestCopyOfOutput().
  void DidCopyOutput(
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks start_time,
      const ThreadSafeCaptureOracle::CaptureFrameCallback& capture_frame_cb,
      scoped_ptr<cc::CopyOutputResult> result);

  // Helper function to update cursor state.
  // |region_in_frame| defines the desktop bound in the captured frame.
  // Returns the current cursor position in captured frame.
  gfx::Point UpdateCursorState(const gfx::Rect& region_in_frame);

  // Clears cursor state.
  void ClearCursorState();

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

  // Cursor state.
  ui::Cursor last_cursor_;
  gfx::Point cursor_hot_point_;
  SkBitmap scaled_cursor_bitmap_;

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

void DesktopVideoCaptureMachine::Stop(const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Stop observing window events.
  if (desktop_window_) {
    desktop_window_->RemoveObserver(this);
    desktop_window_ = NULL;
  }

  // Stop observing compositor updates.
  if (desktop_layer_) {
    ui::Compositor* compositor = desktop_layer_->GetCompositor();
    if (compositor)
      compositor->RemoveObserver(this);
    desktop_layer_ = NULL;
  }

  // Stop timer.
  timer_.Stop();

  started_ = false;

  callback.Run();
}

void DesktopVideoCaptureMachine::UpdateCaptureSize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (oracle_proxy_ && desktop_layer_) {
    oracle_proxy_->UpdateCaptureSize(ui::ConvertSizeToPixel(
        desktop_layer_, desktop_layer_->bounds().size()));
  }
  ClearCursorState();
}

void DesktopVideoCaptureMachine::Capture(bool dirty) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do not capture if the desktop layer is already destroyed.
  if (!desktop_layer_)
    return;

  scoped_refptr<media::VideoFrame> frame;
  ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  const base::TimeTicks start_time = base::TimeTicks::Now();
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

void CopyOutputFinishedForVideo(
    base::TimeTicks start_time,
    const ThreadSafeCaptureOracle::CaptureFrameCallback& capture_frame_cb,
    const scoped_refptr<media::VideoFrame>& target,
    const SkBitmap& cursor_bitmap,
    const gfx::Point& cursor_position,
    scoped_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  if (!cursor_bitmap.isNull())
    RenderCursorOnVideoFrame(target, cursor_bitmap, cursor_position);
  release_callback->Run(0, false);
  capture_frame_cb.Run(start_time, result);
}

void DesktopVideoCaptureMachine::DidCopyOutput(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks start_time,
    const ThreadSafeCaptureOracle::CaptureFrameCallback& capture_frame_cb,
    scoped_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty() || result->size().IsEmpty() || !desktop_layer_)
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

  gfx::Point cursor_position_in_frame = UpdateCursorState(region_in_frame);
  yuv_readback_pipeline_->ReadbackYUV(
      texture_mailbox.mailbox(),
      texture_mailbox.sync_point(),
      video_frame.get(),
      base::Bind(&CopyOutputFinishedForVideo,
                 start_time,
                 capture_frame_cb,
                 video_frame,
                 scaled_cursor_bitmap_,
                 cursor_position_in_frame,
                 base::Passed(&release_callback)));
}

gfx::Point DesktopVideoCaptureMachine::UpdateCursorState(
    const gfx::Rect& region_in_frame) {
  const gfx::Rect desktop_bounds = desktop_layer_->bounds();
  gfx::NativeCursor cursor =
      desktop_window_->GetHost()->last_cursor();
  if (last_cursor_ != cursor) {
    SkBitmap cursor_bitmap;
    if (ui::GetCursorBitmap(cursor, &cursor_bitmap, &cursor_hot_point_)) {
      scaled_cursor_bitmap_ = skia::ImageOperations::Resize(
          cursor_bitmap,
          skia::ImageOperations::RESIZE_BEST,
          cursor_bitmap.width() * region_in_frame.width() /
              desktop_bounds.width(),
          cursor_bitmap.height() * region_in_frame.height() /
              desktop_bounds.height());
      last_cursor_ = cursor;
    } else {
      // Clear cursor state if ui::GetCursorBitmap failed so that we do not
      // render cursor on the captured frame.
      ClearCursorState();
    }
  }

  gfx::Point cursor_position = aura::Env::GetInstance()->last_mouse_location();
  const gfx::Point hot_point_in_dip = ui::ConvertPointToDIP(
      desktop_layer_, cursor_hot_point_);
  cursor_position.Offset(-desktop_bounds.x() - hot_point_in_dip.x(),
                         -desktop_bounds.y() - hot_point_in_dip.y());
  return gfx::Point(
      region_in_frame.x() + cursor_position.x() * region_in_frame.width() /
          desktop_bounds.width(),
      region_in_frame.y() + cursor_position.y() * region_in_frame.height() /
          desktop_bounds.height());
}

void DesktopVideoCaptureMachine::ClearCursorState() {
  last_cursor_ = ui::Cursor();
  cursor_hot_point_ = gfx::Point();
  scaled_cursor_bitmap_.reset();
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Stop(base::Bind(&base::DoNothing));

  oracle_proxy_->ReportError("OnWindowDestroyed()");
}

void DesktopVideoCaptureMachine::OnCompositingEnded(
    ui::Compositor* compositor) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &DesktopVideoCaptureMachine::Capture, AsWeakPtr(), true));
}

}  // namespace

DesktopCaptureDeviceAura::DesktopCaptureDeviceAura(
    const DesktopMediaID& source)
    : core_(new ContentVideoCaptureDeviceCore(scoped_ptr<VideoCaptureMachine>(
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
  core_->AllocateAndStart(params, client.Pass());
}

void DesktopCaptureDeviceAura::StopAndDeAllocate() {
  core_->StopAndDeAllocate();
}

}  // namespace content
