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

#if !defined(AVOID_LIBYUV_FOR_ANDROID_WEBVIEW)
#include "third_party/libyuv/include/libyuv.h"
#endif

using media::VideoCaptureCapability;

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

  // Buffers that are currently known to this client.
  std::set<int> known_buffers;

  // Buffers currently held by this client.
  std::set<int> active_buffers;

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
    : public media::VideoCaptureDevice::Client {
 public:
  explicit VideoCaptureDeviceClient(
      const base::WeakPtr<VideoCaptureController>& controller,
      const scoped_refptr<VideoCaptureBufferPool>& buffer_pool);
  virtual ~VideoCaptureDeviceClient();

  // VideoCaptureDevice::Client implementation.
  virtual scoped_refptr<media::VideoFrame> ReserveOutputBuffer(
      const gfx::Size& size) OVERRIDE;
  virtual void OnIncomingCapturedFrame(
      const uint8* data,
      int length,
      base::Time timestamp,
      int rotation,
      bool flip_vert,
      bool flip_horiz,
      const VideoCaptureCapability& frame_info) OVERRIDE;
  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      base::Time timestamp,
      int frame_rate) OVERRIDE;
  virtual void OnError() OVERRIDE;

 private:
  scoped_refptr<media::VideoFrame> DoReserveI420VideoFrame(
      const gfx::Size& size,
      int rotation);

  // The controller to which we post events.
  const base::WeakPtr<VideoCaptureController> controller_;

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;
};

VideoCaptureController::VideoCaptureController()
    : buffer_pool_(new VideoCaptureBufferPool(kNoOfBuffers)),
      state_(VIDEO_CAPTURE_STATE_STARTED),
      weak_ptr_factory_(this) {
}

VideoCaptureController::VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool)
    : controller_(controller),
      buffer_pool_(buffer_pool) {}

VideoCaptureController::VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {}

base::WeakPtr<VideoCaptureController> VideoCaptureController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_ptr<media::VideoCaptureDevice::Client>
VideoCaptureController::NewDeviceClient() {
  scoped_ptr<media::VideoCaptureDevice::Client> result(
      new VideoCaptureDeviceClient(this->GetWeakPtr(), buffer_pool_));
  return result.Pass();
}

void VideoCaptureController::AddClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    base::ProcessHandle render_process,
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::AddClient, id " << id.device_id
           << ", (" << params.requested_format.width
           << ", " << params.requested_format.height
           << ", " << params.requested_format.frame_rate
           << ", " << params.session_id
           << ")";

  // If this is the first client added to the controller, cache the parameters.
  if (!controller_clients_.size())
    video_capture_format_ = params.requested_format;

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
  for (std::set<int>::iterator buffer_it = client->active_buffers.begin();
       buffer_it != client->active_buffers.end();
       ++buffer_it) {
    int buffer_id = *buffer_it;
    buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
  }
  client->active_buffers.clear();

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
  if (!client || !client->active_buffers.erase(buffer_id)) {
    NOTREACHED();
    return;
  }

  buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
}

const media::VideoCaptureFormat&
VideoCaptureController::GetVideoCaptureFormat() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return video_capture_format_;
}

scoped_refptr<media::VideoFrame>
VideoCaptureController::VideoCaptureDeviceClient::ReserveOutputBuffer(
    const gfx::Size& size) {
  return DoReserveI420VideoFrame(size, 0);
}

void VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    base::Time timestamp,
    int rotation,
    bool flip_vert,
    bool flip_horiz,
    const VideoCaptureCapability& frame_info) {
  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedFrame");

  if (!frame_info.IsValid())
    return;

  // Chopped pixels in width/height in case video capture device has odd
  // numbers for width/height.
  int chopped_width = 0;
  int chopped_height = 0;
  int new_width = frame_info.width;
  int new_height = frame_info.height;

  if (frame_info.width & 1) {
    --new_width;
    chopped_width = 1;
  }
  if (frame_info.height & 1) {
    --new_height;
    chopped_height = 1;
  }

  scoped_refptr<media::VideoFrame> dst = DoReserveI420VideoFrame(
      gfx::Size(new_width, new_height), rotation);

  if (!dst.get())
    return;
#if !defined(AVOID_LIBYUV_FOR_ANDROID_WEBVIEW)

  uint8* yplane = dst->data(media::VideoFrame::kYPlane);
  uint8* uplane = dst->data(media::VideoFrame::kUPlane);
  uint8* vplane = dst->data(media::VideoFrame::kVPlane);
  int yplane_stride = new_width;
  int uv_plane_stride = (new_width + 1) / 2;
  int crop_x = 0;
  int crop_y = 0;
  int destination_width = new_width;
  int destination_height = new_height;
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

  switch (frame_info.color) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_I420;
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YV12;
      break;
    case media::PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV12;
      break;
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YUY2;
      break;
    case media::PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width && !chopped_height);
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
  if (frame_info.color == media::PIXEL_FORMAT_RGB24) {
    // Rotation and flipping is not supported in kRGB24 and OS_WIN case.
    DCHECK(!rotation && !flip_vert && !flip_horiz);
    need_convert_rgb24_on_win = true;
  }
#endif
  if (need_convert_rgb24_on_win) {
    int rgb_stride = -3 * (new_width + chopped_width);
    const uint8* rgb_src =
        data + 3 * (new_width + chopped_width) *
                   (new_height - 1 + chopped_height);
    media::ConvertRGB24ToYUV(rgb_src,
                             yplane,
                             uplane,
                             vplane,
                             new_width,
                             new_height,
                             rgb_stride,
                             yplane_stride,
                             uv_plane_stride);
  } else {
    if (new_rotation_angle==90 || new_rotation_angle==270){
      // To be compatible with non-libyuv code in RotatePlaneByPixels, when
      // rotating by 90/270, only the maximum square portion located in the
      // center of the image is rotated. F.i. 640x480 pixels, only the central
      // 480 pixels would be rotated and the leftmost and rightmost 80 columns
      // would be ignored. This process is called letterboxing.
      int letterbox_thickness = abs(new_width - new_height) / 2;
      if (destination_width > destination_height) {
        yplane += letterbox_thickness;
        uplane += letterbox_thickness / 2;
        vplane += letterbox_thickness / 2;
        destination_width = destination_height;
      } else {
        yplane += letterbox_thickness * destination_width;
        uplane += (letterbox_thickness * destination_width) / 2;
        vplane += (letterbox_thickness * destination_width) / 2;
        destination_height = destination_width;
      }
    }
    libyuv::ConvertToI420(
        data, length,
        yplane, yplane_stride,
        uplane, uv_plane_stride,
        vplane, uv_plane_stride,
        crop_x, crop_y,
        new_width + chopped_width,
        new_height * (flip_vert ^ flip_horiz ? -1 : 1),
        destination_width,
        destination_height,
        rotation_mode,
        origin_colorspace);
  }
#else
  // Libyuv is not linked in for Android WebView builds, but video capture is
  // not used in those builds either. Whenever libyuv is added in that build,
  // address all these #ifdef parts, see http://crbug.com/299611 .
  NOTREACHED();
#endif  // if !defined(AVOID_LIBYUV_FOR_ANDROID_WEBVIEW)
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                 controller_,
                 dst,
                 frame_info.frame_rate,
                 timestamp));
}

void
VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    base::Time timestamp,
    int frame_rate) {
  // If this is a frame that belongs to the buffer pool, we can forward it
  // directly to the IO thread and be done.
  if (buffer_pool_->RecognizeReservedBuffer(
      frame->shared_memory_handle()) >= 0) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                   controller_, frame, frame_rate, timestamp));
    return;
  }

  NOTREACHED() << "Frames should always belong to the buffer pool.";
}

void VideoCaptureController::VideoCaptureDeviceClient::OnError() {
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

scoped_refptr<media::VideoFrame>
VideoCaptureController::VideoCaptureDeviceClient::DoReserveI420VideoFrame(
    const gfx::Size& size,
    int rotation) {
  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  scoped_refptr<media::VideoFrame> frame =
      buffer_pool_->ReserveI420VideoFrame(size, rotation, &buffer_id_to_drop);
  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoBufferDestroyedOnIOThread,
                   controller_, buffer_id_to_drop));
  }
  return frame;
}

VideoCaptureController::~VideoCaptureController() {
  STLDeleteContainerPointers(controller_clients_.begin(),
                             controller_clients_.end());
}

void VideoCaptureController::DoIncomingCapturedFrameOnIOThread(
    const scoped_refptr<media::VideoFrame>& reserved_frame,
    int frame_rate,
    base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int buffer_id = buffer_pool_->RecognizeReservedBuffer(
      reserved_frame->shared_memory_handle());
  if (buffer_id < 0) {
    NOTREACHED();
    return;
  }

  media::VideoCaptureFormat frame_format(
      reserved_frame->coded_size().width(),
      reserved_frame->coded_size().height(),
      frame_rate,
      media::VariableResolutionVideoCaptureDevice);

  int count = 0;
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); ++client_it) {
      ControllerClient* client = *client_it;
      if (client->session_closed)
        continue;

      bool is_new_buffer = client->known_buffers.insert(buffer_id).second;
      if (is_new_buffer) {
        // On the first use of a buffer on a client, share the memory handle.
        size_t memory_size = 0;
        base::SharedMemoryHandle remote_handle = buffer_pool_->ShareToProcess(
            buffer_id, client->render_process_handle, &memory_size);
        client->event_handler->OnBufferCreated(client->controller_id,
                                               remote_handle,
                                               memory_size,
                                               buffer_id);
      }

      client->event_handler->OnBufferReady(client->controller_id,
                                           buffer_id, timestamp,
                                           frame_format);
      bool inserted = client->active_buffers.insert(buffer_id).second;
      DCHECK(inserted) << "Unexpected duplicate buffer: " << buffer_id;
      count++;
    }
  }

  buffer_pool_->HoldForConsumers(buffer_id, count);
}

void VideoCaptureController::DoErrorOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state_ = VIDEO_CAPTURE_STATE_ERROR;

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    ControllerClient* client = *client_it;
    if (client->session_closed)
       continue;

    client->event_handler->OnError(client->controller_id);
  }
}

void VideoCaptureController::DoBufferDestroyedOnIOThread(
    int buffer_id_to_drop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    ControllerClient* client = *client_it;
    if (client->session_closed)
      continue;

    if (client->known_buffers.erase(buffer_id_to_drop)) {
      client->event_handler->OnBufferDestroyed(client->controller_id,
                                               buffer_id_to_drop);
    }
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
