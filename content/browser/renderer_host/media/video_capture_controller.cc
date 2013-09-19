// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <set>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"

#if !defined(OS_IOS) && !defined(OS_ANDROID)
#include "third_party/libyuv/include/libyuv.h"
#endif

namespace {

#if defined(OS_IOS) || defined(OS_ANDROID)
// TODO(wjia): Support stride.
void RotatePackedYV12Frame(
    const uint8* src,
    uint8* dest_yplane,
    uint8* dest_uplane,
    uint8* dest_vplane,
    int width,
    int height,
    int rotation,
    bool flip_vert,
    bool flip_horiz) {
  media::RotatePlaneByPixels(
      src, dest_yplane, width, height, rotation, flip_vert, flip_horiz);
  int y_size = width * height;
  src += y_size;
  media::RotatePlaneByPixels(
      src, dest_uplane, width/2, height/2, rotation, flip_vert, flip_horiz);
  src += y_size/4;
  media::RotatePlaneByPixels(
      src, dest_vplane, width/2, height/2, rotation, flip_vert, flip_horiz);
}
#endif  // #if defined(OS_IOS) || defined(OS_ANDROID)

}  // namespace

namespace content {

// The number of buffers that VideoCaptureBufferPool should allocate.
static const int kNoOfBuffers = 3;

struct VideoCaptureController::ControllerClient {
  ControllerClient(
      const VideoCaptureControllerID& id,
      VideoCaptureControllerEventHandler* handler,
      base::ProcessHandle render_process,
      const media::VideoCaptureParams& params)
      : controller_id(id),
        event_handler(handler),
        render_process_handle(render_process),
        parameters(params),
        session_closed(false) {
  }

  ~ControllerClient() {}

  // ID used for identifying this object.
  VideoCaptureControllerID controller_id;
  VideoCaptureControllerEventHandler* event_handler;

  // Handle to the render process that will receive the capture buffers.
  base::ProcessHandle render_process_handle;
  media::VideoCaptureParams parameters;

  // Buffers used by this client.
  std::set<int> buffers;

  // State of capture session, controlled by VideoCaptureManager directly. This
  // transitions to true as soon as StopSession() occurs, at which point the
  // client is sent an OnEnded() event. However, because the client retains a
  // VideoCaptureController* pointer, its ControllerClient entry lives on until
  // it unregisters itself via RemoveClient(), which may happen asynchronously.
  //
  // TODO(nick): If we changed the semantics of VideoCaptureHost so that
  // OnEnded() events were processed synchronously (with the RemoveClient() done
  // implicitly), we could avoid tracking this state here in the Controller, and
  // simplify the code in both places.
  bool session_closed;
};

// Receives events from the VideoCaptureDevice and posts them to a
// VideoCaptureController on the IO thread. An instance of this class may safely
// outlive its target VideoCaptureController.
//
// Methods of this class may be called from any thread, and in practice will
// often be called on some auxiliary thread depending on the platform and the
// device type; including, for example, the DirectShow thread on Windows, the
// v4l2_thread on Linux, and the UI thread for tab capture.
class VideoCaptureController::VideoCaptureDeviceClient
    : public media::VideoCaptureDevice::EventHandler {
 public:
  explicit VideoCaptureDeviceClient(
      const base::WeakPtr<VideoCaptureController>& controller);
  virtual ~VideoCaptureDeviceClient();

  // VideoCaptureDevice::EventHandler implementation.
  virtual scoped_refptr<media::VideoFrame> ReserveOutputBuffer() OVERRIDE;
  virtual void OnIncomingCapturedFrame(const uint8* data,
                                       int length,
                                       base::Time timestamp,
                                       int rotation,
                                       bool flip_vert,
                                       bool flip_horiz) OVERRIDE;
  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      base::Time timestamp) OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnFrameInfo(
      const media::VideoCaptureCapability& info) OVERRIDE;
  virtual void OnFrameInfoChanged(
      const media::VideoCaptureCapability& info) OVERRIDE;

 private:
  // The controller to which we post events.
  const base::WeakPtr<VideoCaptureController> controller_;

  // The pool of shared-memory buffers used for capturing.
  scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // Chopped pixels in width/height in case video capture device has odd
  // numbers for width/height.
  int chopped_width_;
  int chopped_height_;

  // Tracks the current frame format.
  media::VideoCaptureCapability frame_info_;

  // For NV21 we have to do color conversion into the intermediate buffer and
  // from there the rotations. This variable won't be needed after
  // http://crbug.com/292400
#if defined(OS_IOS) || defined(OS_ANDROID)
  scoped_ptr<uint8[]> i420_intermediate_buffer_;
#endif  // #if defined(OS_IOS) || defined(OS_ANDROID)
};

VideoCaptureController::VideoCaptureController()
    : state_(VIDEO_CAPTURE_STATE_STARTED),
      weak_ptr_factory_(this) {
  memset(&current_params_, 0, sizeof(current_params_));
}

VideoCaptureController::VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller)
    : controller_(controller),
      chopped_width_(0),
      chopped_height_(0) {}

VideoCaptureController::VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {}

base::WeakPtr<VideoCaptureController> VideoCaptureController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_ptr<media::VideoCaptureDevice::EventHandler>
VideoCaptureController::NewDeviceClient() {
  scoped_ptr<media::VideoCaptureDevice::EventHandler> result(
      new VideoCaptureDeviceClient(this->GetWeakPtr()));
  return result.Pass();
}

void VideoCaptureController::AddClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    base::ProcessHandle render_process,
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::AddClient, id " << id.device_id
           << ", (" << params.width
           << ", " << params.height
           << ", " << params.frame_rate
           << ", " << params.session_id
           << ")";

  // Signal error in case device is already in error state.
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(id);
    return;
  }

  // Do nothing if this client has called AddClient before.
  if (FindClient(id, event_handler, controller_clients_))
    return;

  ControllerClient* client = new ControllerClient(id, event_handler,
                                                  render_process, params);
  // If we already have gotten frame_info from the device, repeat it to the new
  // client.
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    if (frame_info_.IsValid()) {
      SendFrameInfoAndBuffers(client);
    }
    controller_clients_.push_back(client);
    return;
  }
}

int VideoCaptureController::RemoveClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::RemoveClient, id " << id.device_id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return kInvalidMediaCaptureSessionId;

  // Take back all buffers held by the |client|.
  if (buffer_pool_.get()) {
    for (std::set<int>::iterator buffer_it = client->buffers.begin();
         buffer_it != client->buffers.end();
         ++buffer_it) {
      int buffer_id = *buffer_it;
      buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
    }
  }
  client->buffers.clear();

  int session_id = client->parameters.session_id;
  controller_clients_.remove(client);
  delete client;

  return session_id;
}

void VideoCaptureController::StopSession(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::StopSession, id " << session_id;

  ControllerClient* client = FindClient(session_id, controller_clients_);

  if (client) {
    client->session_closed = true;
    client->event_handler->OnEnded(client->controller_id);
  }
}

void VideoCaptureController::ReturnBuffer(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);

  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  if (!client ||
      client->buffers.find(buffer_id) == client->buffers.end()) {
    NOTREACHED();
    return;
  }

  client->buffers.erase(buffer_id);
  buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
}

scoped_refptr<media::VideoFrame>
VideoCaptureController::VideoCaptureDeviceClient::ReserveOutputBuffer() {
  return buffer_pool_->ReserveI420VideoFrame(gfx::Size(frame_info_.width,
                                                       frame_info_.height),
                                             0);
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
void VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    base::Time timestamp,
    int rotation,
    bool flip_vert,
    bool flip_horiz) {
  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedFrame");

  if (!buffer_pool_.get())
    return;
  scoped_refptr<media::VideoFrame> dst = buffer_pool_->ReserveI420VideoFrame(
      gfx::Size(frame_info_.width, frame_info_.height), rotation);

  if (!dst.get())
    return;

  uint8* yplane = dst->data(media::VideoFrame::kYPlane);
  uint8* uplane = dst->data(media::VideoFrame::kUPlane);
  uint8* vplane = dst->data(media::VideoFrame::kVPlane);
  int yplane_stride = frame_info_.width;
  int uv_plane_stride = (frame_info_.width + 1) / 2;
  int crop_x = 0;
  int crop_y = 0;
  libyuv::FourCC origin_colorspace = libyuv::FOURCC_ANY;
  // Assuming rotation happens first and flips next, we can consolidate both
  // vertical and horizontal flips together with rotation into two variables:
  // new_rotation = (rotation + 180 * vertical_flip) modulo 360
  // new_vertical_flip = horizontal_flip XOR vertical_flip
  int new_rotation_angle = (rotation + 180 * flip_vert) % 360;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (new_rotation_angle == 90)
    rotation_mode = libyuv::kRotate90;
  else if (new_rotation_angle == 180)
    rotation_mode = libyuv::kRotate180;
  else if (new_rotation_angle == 270)
    rotation_mode = libyuv::kRotate270;

  switch (frame_info_.color) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width_ && !chopped_height_);
      origin_colorspace = libyuv::FOURCC_I420;
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width_ && !chopped_height_);
      origin_colorspace = libyuv::FOURCC_YV12;
      break;
    case media::PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width_ && !chopped_height_);
      origin_colorspace = libyuv::FOURCC_NV12;
      break;
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width_ && !chopped_height_);
      origin_colorspace = libyuv::FOURCC_YUY2;
      break;
    case media::PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width_ && !chopped_height_);
      origin_colorspace = libyuv::FOURCC_UYVY;
      break;
    case media::PIXEL_FORMAT_RGB24:
      origin_colorspace = libyuv::FOURCC_RAW;
      break;
    case media::PIXEL_FORMAT_ARGB:
      origin_colorspace = libyuv::FOURCC_ARGB;
      break;
    case media::PIXEL_FORMAT_MJPEG:
      origin_colorspace = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }

  int need_convert_rgb24_on_win = false;
#if defined(OS_WIN)
  // kRGB24 on Windows start at the bottom line and has a negative stride. This
  // is not supported by libyuv, so the media API is used instead.
  if (frame_info_.color == media::PIXEL_FORMAT_RGB24) {
    // Rotation and flipping is not supported in kRGB24 and OS_WIN case.
    DCHECK(!rotation && !flip_vert && !flip_horiz);
    need_convert_rgb24_on_win = true;
  }
#endif
  if (need_convert_rgb24_on_win) {
    int rgb_stride = -3 * (frame_info_.width + chopped_width_);
    const uint8* rgb_src =
        data + 3 * (frame_info_.width + chopped_width_) *
                   (frame_info_.height - 1 + chopped_height_);
    media::ConvertRGB24ToYUV(rgb_src,
                             yplane,
                             uplane,
                             vplane,
                             frame_info_.width,
                             frame_info_.height,
                             rgb_stride,
                             yplane_stride,
                             uv_plane_stride);
  } else {
    libyuv::ConvertToI420(
        data,
        length,
        yplane,
        yplane_stride,
        uplane,
        uv_plane_stride,
        vplane,
        uv_plane_stride,
        crop_x,
        crop_y,
        frame_info_.width + chopped_width_,
        frame_info_.height * (flip_vert ^ flip_horiz ? -1 : 1),
        frame_info_.width,
        frame_info_.height,
        rotation_mode,
        origin_colorspace);
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                 controller_,
                 dst,
                 timestamp));
}
#else
void VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    base::Time timestamp,
    int rotation,
    bool flip_vert,
    bool flip_horiz) {
  DCHECK(frame_info_.color == media::PIXEL_FORMAT_I420 ||
         frame_info_.color == media::PIXEL_FORMAT_YV12 ||
         frame_info_.color == media::PIXEL_FORMAT_NV21 ||
         (rotation == 0 && !flip_vert && !flip_horiz));

  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedFrame");

  if (!buffer_pool_)
    return;
  scoped_refptr<media::VideoFrame> dst =
      buffer_pool_->ReserveI420VideoFrame(gfx::Size(frame_info_.width,
                                                    frame_info_.height),
                                          rotation);

  if (!dst.get())
    return;

  uint8* yplane = dst->data(media::VideoFrame::kYPlane);
  uint8* uplane = dst->data(media::VideoFrame::kUPlane);
  uint8* vplane = dst->data(media::VideoFrame::kVPlane);

  // Do color conversion from the camera format to I420.
  switch (frame_info_.color) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width_ && !chopped_height_);
      RotatePackedYV12Frame(
          data, yplane, uplane, vplane, frame_info_.width, frame_info_.height,
          rotation, flip_vert, flip_horiz);
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width_ && !chopped_height_);
      RotatePackedYV12Frame(
          data, yplane, vplane, uplane, frame_info_.width, frame_info_.height,
          rotation, flip_vert, flip_horiz);
      break;
    case media::PIXEL_FORMAT_NV21: {
      DCHECK(!chopped_width_ && !chopped_height_);
      int num_pixels = frame_info_.width * frame_info_.height;
      media::ConvertNV21ToYUV(data,
                              &i420_intermediate_buffer_[0],
                              &i420_intermediate_buffer_[num_pixels],
                              &i420_intermediate_buffer_[num_pixels * 5 / 4],
                              frame_info_.width,
                              frame_info_.height);
      RotatePackedYV12Frame(
          i420_intermediate_buffer_.get(), yplane, uplane, vplane,
          frame_info_.width, frame_info_.height,
          rotation, flip_vert, flip_horiz);
       break;
    }
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width_ && !chopped_height_);
      if (frame_info_.width * frame_info_.height * 2 != length) {
        // If |length| of |data| does not match the expected width and height
        // we can't convert the frame to I420. YUY2 is 2 bytes per pixel.
        break;
      }

      media::ConvertYUY2ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                              frame_info_.height);
      break;
    case media::PIXEL_FORMAT_RGB24: {
      int ystride = frame_info_.width;
      int uvstride = frame_info_.width / 2;
      int rgb_stride = 3 * (frame_info_.width + chopped_width_);
      const uint8* rgb_src = data;
      media::ConvertRGB24ToYUV(rgb_src, yplane, uplane, vplane,
                               frame_info_.width, frame_info_.height,
                               rgb_stride, ystride, uvstride);
      break;
    }
    case media::PIXEL_FORMAT_ARGB:
      media::ConvertRGB32ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                               frame_info_.height,
                               (frame_info_.width + chopped_width_) * 4,
                               frame_info_.width, frame_info_.width / 2);
      break;
    default:
      NOTREACHED();
  }

  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                 controller_, dst, timestamp));
}
#endif  // #if !defined(OS_IOS) && !defined(OS_ANDROID)

void
VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    base::Time timestamp) {
  if (!buffer_pool_)
    return;

  // If this is a frame that belongs to the buffer pool, we can forward it
  // directly to the IO thread and be done.
  if (buffer_pool_->RecognizeReservedBuffer(
      frame->shared_memory_handle()) >= 0) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                   controller_, frame, timestamp));
    return;
  }

  // Otherwise, this is a frame that belongs to the caller, and we must copy
  // it to a frame from the buffer pool.
  scoped_refptr<media::VideoFrame> target =
      buffer_pool_->ReserveI420VideoFrame(gfx::Size(frame_info_.width,
                                                    frame_info_.height),
                                          0);

  if (!target.get())
    return;

  // Validate the inputs.
  if (frame->coded_size() != target->coded_size())
    return;  // Only exact copies are supported.
  if (!(frame->format() == media::VideoFrame::I420 ||
        frame->format() == media::VideoFrame::YV12 ||
        frame->format() == media::VideoFrame::RGB32)) {
    NOTREACHED() << "Unsupported format passed to OnIncomingCapturedVideoFrame";
    return;
  }

  const int kYPlane = media::VideoFrame::kYPlane;
  const int kUPlane = media::VideoFrame::kUPlane;
  const int kVPlane = media::VideoFrame::kVPlane;
  const int kAPlane = media::VideoFrame::kAPlane;
  const int kRGBPlane = media::VideoFrame::kRGBPlane;

  // Do color conversion from the camera format to I420.
  switch (frame->format()) {
#if defined(GOOGLE_TV)
    case media::VideoFrame::HOLE:
      // Fall-through to NOTREACHED() block.
#endif
    case media::VideoFrame::INVALID:
    case media::VideoFrame::YV16:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::NATIVE_TEXTURE: {
      NOTREACHED();
      break;
    }
    case media::VideoFrame::I420:
    case media::VideoFrame::YV12: {
      DCHECK(!chopped_width_ && !chopped_height_);
      media::CopyYPlane(frame->data(kYPlane),
                        frame->stride(kYPlane),
                        frame->rows(kYPlane),
                        target.get());
      media::CopyUPlane(frame->data(kUPlane),
                        frame->stride(kUPlane),
                        frame->rows(kUPlane),
                        target.get());
      media::CopyVPlane(frame->data(kVPlane),
                        frame->stride(kVPlane),
                        frame->rows(kVPlane),
                        target.get());
      break;
    }
    case media::VideoFrame::YV12A: {
      DCHECK(!chopped_width_ && !chopped_height_);
      media::CopyYPlane(frame->data(kYPlane),
                        frame->stride(kYPlane),
                        frame->rows(kYPlane),
                        target.get());
      media::CopyUPlane(frame->data(kUPlane),
                        frame->stride(kUPlane),
                        frame->rows(kUPlane),
                        target.get());
      media::CopyVPlane(frame->data(kVPlane),
                        frame->stride(kVPlane),
                        frame->rows(kVPlane),
                        target.get());
      media::CopyAPlane(frame->data(kAPlane),
                        frame->stride(kAPlane),
                        frame->rows(kAPlane),
                        target.get());
      break;
    }
    case media::VideoFrame::RGB32: {
      media::ConvertRGB32ToYUV(frame->data(kRGBPlane),
                               target->data(kYPlane),
                               target->data(kUPlane),
                               target->data(kVPlane),
                               target->coded_size().width(),
                               target->coded_size().height(),
                               frame->stride(kRGBPlane),
                               target->stride(kYPlane),
                               target->stride(kUPlane));
      break;
    }
  }

  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                 controller_, target, timestamp));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnError() {
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnFrameInfo(
    const media::VideoCaptureCapability& info) {
  frame_info_ = info;
  // Handle cases when |info| has odd numbers for width/height.
  if (info.width & 1) {
    --frame_info_.width;
    chopped_width_ = 1;
  } else {
    chopped_width_ = 0;
  }
  if (info.height & 1) {
    --frame_info_.height;
    chopped_height_ = 1;
  } else {
    chopped_height_ = 0;
  }
#if defined(OS_IOS) || defined(OS_ANDROID)
  if (frame_info_.color == media::PIXEL_FORMAT_NV21 &&
      !i420_intermediate_buffer_) {
    i420_intermediate_buffer_.reset(
        new uint8[frame_info_.width * frame_info_.height * 12 / 8]);
  }
#endif  // #if defined(OS_IOS) || defined(OS_ANDROID)

  DCHECK(!buffer_pool_.get());

  // TODO(nick): Give BufferPool the same lifetime as the controller, have it
  // support frame size changes, and stop checking it for NULL everywhere.
  // http://crbug.com/266082
  buffer_pool_ = new VideoCaptureBufferPool(
      media::VideoFrame::AllocationSize(
            media::VideoFrame::I420,
            gfx::Size(frame_info_.width, frame_info_.height)),
      kNoOfBuffers);

  // Check whether all buffers were created successfully.
  if (!buffer_pool_->Allocate()) {
    // Transition to the error state.
    buffer_pool_ = NULL;
    OnError();
    return;
  }

  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoFrameInfoOnIOThread, controller_,
                 frame_info_, buffer_pool_));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnFrameInfoChanged(
    const media::VideoCaptureCapability& info) {
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoFrameInfoChangedOnIOThread,
                 controller_, info));
}

VideoCaptureController::~VideoCaptureController() {
  buffer_pool_ = NULL;  // Release all buffers.
  STLDeleteContainerPointers(controller_clients_.begin(),
                             controller_clients_.end());
}

void VideoCaptureController::DoIncomingCapturedFrameOnIOThread(
    const scoped_refptr<media::VideoFrame>& reserved_frame,
    base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!buffer_pool_.get())
    return;

  int buffer_id = buffer_pool_->RecognizeReservedBuffer(
      reserved_frame->shared_memory_handle());
  if (buffer_id < 0) {
    NOTREACHED();
    return;
  }

  int count = 0;
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); ++client_it) {
      if ((*client_it)->session_closed)
        continue;

      (*client_it)->event_handler->OnBufferReady((*client_it)->controller_id,
                                                 buffer_id, timestamp);
      (*client_it)->buffers.insert(buffer_id);
      count++;
    }
  }

  buffer_pool_->HoldForConsumers(buffer_id, count);
}

void VideoCaptureController::DoFrameInfoOnIOThread(
    const media::VideoCaptureCapability& frame_info,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!buffer_pool_.get()) << "Frame info should happen only once.";

  // Allocate memory only when device has been started.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;

  frame_info_ = frame_info;
  buffer_pool_ = buffer_pool;

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    if ((*client_it)->session_closed)
        continue;

    SendFrameInfoAndBuffers(*client_it);
  }
}

void VideoCaptureController::DoFrameInfoChangedOnIOThread(
    const media::VideoCaptureCapability& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // TODO(mcasas): Here we should reallocate the VideoCaptureBufferPool, if
  // needed, to support the new video capture format. See crbug.com/266082.
  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    if ((*client_it)->session_closed)
        continue;

    (*client_it)->event_handler->OnFrameInfoChanged(
        (*client_it)->controller_id,
        info.width,
        info.height,
        info.frame_rate);
  }
}

void VideoCaptureController::DoErrorOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state_ = VIDEO_CAPTURE_STATE_ERROR;
  ControllerClients::iterator client_it;
  for (client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
     if ((*client_it)->session_closed)
        continue;

    (*client_it)->event_handler->OnError((*client_it)->controller_id);
  }
}

void VideoCaptureController::SendFrameInfoAndBuffers(ControllerClient* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(frame_info_.IsValid());
  client->event_handler->OnFrameInfo(client->controller_id,
                                     frame_info_);
  for (int buffer_id = 0; buffer_id < buffer_pool_->count(); ++buffer_id) {
    base::SharedMemoryHandle remote_handle =
        buffer_pool_->ShareToProcess(buffer_id, client->render_process_handle);

    client->event_handler->OnBufferCreated(client->controller_id,
                                           remote_handle,
                                           buffer_pool_->GetMemorySize(),
                                           buffer_id);
  }
}

VideoCaptureController::ControllerClient*
VideoCaptureController::FindClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* handler,
    const ControllerClients& clients) {
  for (ControllerClients::const_iterator client_it = clients.begin();
       client_it != clients.end(); ++client_it) {
    if ((*client_it)->controller_id == id &&
        (*client_it)->event_handler == handler) {
      return *client_it;
    }
  }
  return NULL;
}

VideoCaptureController::ControllerClient*
VideoCaptureController::FindClient(
    int session_id,
    const ControllerClients& clients) {
  for (ControllerClients::const_iterator client_it = clients.begin();
       client_it != clients.end(); ++client_it) {
    if ((*client_it)->parameters.session_id == session_id) {
      return *client_it;
    }
  }
  return NULL;
}

int VideoCaptureController::GetClientCount() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return controller_clients_.size();
}

}  // namespace content
