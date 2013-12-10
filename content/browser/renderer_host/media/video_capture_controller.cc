// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <set>

#include "base/bind.h"
#include "base/debug/trace_event.h"
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

using media::VideoCaptureFormat;

namespace content {

namespace {

// The number of buffers that VideoCaptureBufferPool should allocate.
const int kNoOfBuffers = 3;

class PoolBuffer : public media::VideoCaptureDevice::Client::Buffer {
 public:
  PoolBuffer(const scoped_refptr<VideoCaptureBufferPool>& pool,
             int buffer_id,
             void* data,
             size_t size)
      : Buffer(buffer_id, data, size), pool_(pool) {
    DCHECK(pool_);
  }

 private:
  virtual ~PoolBuffer() { pool_->RelinquishProducerReservation(id()); }

  const scoped_refptr<VideoCaptureBufferPool> pool_;
};

}  // anonymous namespace

struct VideoCaptureController::ControllerClient {
  ControllerClient(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* handler,
                   base::ProcessHandle render_process,
                   media::VideoCaptureSessionId session_id,
                   const media::VideoCaptureParams& params)
      : controller_id(id),
        event_handler(handler),
        render_process_handle(render_process),
        session_id(session_id),
        parameters(params),
        session_closed(false) {}

  ~ControllerClient() {}

  // ID used for identifying this object.
  const VideoCaptureControllerID controller_id;
  VideoCaptureControllerEventHandler* const event_handler;

  // Handle to the render process that will receive the capture buffers.
  const base::ProcessHandle render_process_handle;
  const media::VideoCaptureSessionId session_id;
  const media::VideoCaptureParams parameters;

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
  virtual scoped_refptr<Buffer> ReserveOutputBuffer(
      media::VideoFrame::Format format,
      const gfx::Size& size) OVERRIDE;
  virtual void OnIncomingCapturedFrame(const uint8* data,
                                       int length,
                                       base::Time timestamp,
                                       int rotation,
                                       bool flip_vert,
                                       bool flip_horiz,
                                       const VideoCaptureFormat& frame_format)
      OVERRIDE;
  virtual void OnIncomingCapturedBuffer(const scoped_refptr<Buffer>& buffer,
                                        media::VideoFrame::Format format,
                                        const gfx::Size& dimensions,
                                        base::Time timestamp,
                                        int frame_rate) OVERRIDE;
  virtual void OnError() OVERRIDE;

 private:
  scoped_refptr<Buffer> DoReserveOutputBuffer(media::VideoFrame::Format format,
                                              const gfx::Size& dimensions,
                                              int rotation);

  // The controller to which we post events.
  const base::WeakPtr<VideoCaptureController> controller_;

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // The set of buffers that have been used for rotated capturing.
  std::set<int> rotated_buffers_;
};

VideoCaptureController::VideoCaptureController()
    : buffer_pool_(new VideoCaptureBufferPool(kNoOfBuffers)),
      state_(VIDEO_CAPTURE_STATE_STARTED),
      weak_ptr_factory_(this) {
}

VideoCaptureController::VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool)
    : controller_(controller), buffer_pool_(buffer_pool) {}

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
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::AddClient, id " << id.device_id
           << ", " << params.requested_format.frame_size.ToString()
           << ", " << params.requested_format.frame_rate
           << ", " << session_id
           << ")";

  // Signal error in case device is already in error state.
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(id);
    return;
  }

  // Do nothing if this client has called AddClient before.
  if (FindClient(id, event_handler, controller_clients_))
    return;

  ControllerClient* client = new ControllerClient(
      id, event_handler, render_process, session_id, params);
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

  int session_id = client->session_id;
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

scoped_refptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureController::VideoCaptureDeviceClient::ReserveOutputBuffer(
    media::VideoFrame::Format format,
    const gfx::Size& size) {
  return DoReserveOutputBuffer(format, size, 0);
}

void VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    base::Time timestamp,
    int rotation,
    bool flip_vert,
    bool flip_horiz,
    const VideoCaptureFormat& frame_format) {
  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedFrame");

  if (!frame_format.IsValid())
    return;

  // Chopped pixels in width/height in case video capture device has odd
  // numbers for width/height.
  int chopped_width = 0;
  int chopped_height = 0;
  int new_width = frame_format.frame_size.width();
  int new_height = frame_format.frame_size.height();

  if (new_width & 1) {
    --new_width;
    chopped_width = 1;
  }
  if (new_height & 1) {
    --new_height;
    chopped_height = 1;
  }

  const gfx::Size dimensions(new_width, new_height);
  scoped_refptr<Buffer> buffer =
      DoReserveOutputBuffer(media::VideoFrame::I420, dimensions, rotation);

  if (!buffer)
    return;
#if !defined(AVOID_LIBYUV_FOR_ANDROID_WEBVIEW)
  uint8* yplane = reinterpret_cast<uint8*>(buffer->data());
  uint8* uplane =
      yplane +
      media::VideoFrame::PlaneAllocationSize(
          media::VideoFrame::I420, media::VideoFrame::kYPlane, dimensions);
  uint8* vplane =
      uplane +
      media::VideoFrame::PlaneAllocationSize(
          media::VideoFrame::I420, media::VideoFrame::kUPlane, dimensions);
  int yplane_stride = new_width;
  int uv_plane_stride = new_width / 2;
  int crop_x = 0;
  int crop_y = 0;
  int destination_width = new_width;
  int destination_height = new_height;
  libyuv::FourCC origin_colorspace = libyuv::FOURCC_ANY;

  // When rotating by 90 and 270 degrees swap |flip_horiz| and |flip_vert|
  // because ConvertToI420() flips image before rotation, while
  // OnIncomingCapturedFrame() interface assumes that rotation happens before
  // flips.
  if (rotation == 90 || rotation == 270)
    std::swap(flip_horiz, flip_vert);

  // Assuming rotation happens first and flips next, we can consolidate both
  // vertical and horizontal flips together with rotation into two variables:
  // new_rotation = (rotation + 180 * horizontal_flip) modulo 360
  // new_vertical_flip = horizontal_flip XOR vertical_flip
  int new_rotation_angle = (rotation + 180 * flip_horiz) % 360;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (new_rotation_angle == 90)
    rotation_mode = libyuv::kRotate90;
  else if (new_rotation_angle == 180)
    rotation_mode = libyuv::kRotate180;
  else if (new_rotation_angle == 270)
    rotation_mode = libyuv::kRotate270;

  switch (frame_format.pixel_format) {
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
      origin_colorspace = libyuv::FOURCC_NV21;
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
  if (frame_format.pixel_format == media::PIXEL_FORMAT_RGB24) {
    // Rotation and flipping is not supported in kRGB24 and OS_WIN case.
    DCHECK(!rotation && !flip_vert && !flip_horiz);
    need_convert_rgb24_on_win = true;
  }
#endif
  if (need_convert_rgb24_on_win) {
    int rgb_stride = -3 * (new_width + chopped_width);
    const uint8* rgb_src = data + 3 * (new_width + chopped_width) *
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
    libyuv::ConvertToI420(data,
                          length,
                          yplane,
                          yplane_stride,
                          uplane,
                          uv_plane_stride,
                          vplane,
                          uv_plane_stride,
                          crop_x,
                          crop_y,
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
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedI420BufferOnIOThread,
          controller_,
          buffer,
          dimensions,
          frame_format.frame_rate,
          timestamp));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    const scoped_refptr<Buffer>& buffer,
    media::VideoFrame::Format format,
    const gfx::Size& dimensions,
    base::Time timestamp,
    int frame_rate) {
  // The capture pipeline expects I420 for now.
  DCHECK_EQ(format, media::VideoFrame::I420)
      << "Non-I420 output buffer returned";

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedI420BufferOnIOThread,
          controller_,
          buffer,
          dimensions,
          frame_rate,
          timestamp));
}

void VideoCaptureController::VideoCaptureDeviceClient::OnError() {
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

scoped_refptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureController::VideoCaptureDeviceClient::DoReserveOutputBuffer(
    media::VideoFrame::Format format,
    const gfx::Size& dimensions,
    int rotation) {
  // The capture pipeline expects I420 for now.
  DCHECK_EQ(format, media::VideoFrame::I420)
      << "Non-I420 output buffer requested";

  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  const size_t frame_bytes =
      media::VideoFrame::AllocationSize(format, dimensions);

  int buffer_id =
      buffer_pool_->ReserveForProducer(frame_bytes, &buffer_id_to_drop);
  if (buffer_id == VideoCaptureBufferPool::kInvalidId)
    return NULL;
  void* data;
  size_t size;
  buffer_pool_->GetBufferInfo(buffer_id, &data, &size);

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> output_buffer(
      new PoolBuffer(buffer_pool_, buffer_id, data, size));

  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoBufferDestroyedOnIOThread,
                   controller_, buffer_id_to_drop));
    rotated_buffers_.erase(buffer_id_to_drop);
  }

  // If a 90/270 rotation is required, letterboxing will be required.  If the
  // returned frame has not been rotated before, then the letterbox borders will
  // not yet have been cleared and we should clear them now.
  if ((rotation % 180) == 0) {
    rotated_buffers_.erase(buffer_id);
  } else {
    if (rotated_buffers_.insert(buffer_id).second) {
      scoped_refptr<media::VideoFrame> frame =
          media::VideoFrame::WrapExternalPackedMemory(
              media::VideoFrame::I420,
              dimensions,
              gfx::Rect(dimensions),
              dimensions,
              static_cast<uint8*>(output_buffer->data()),
              output_buffer->size(),
              base::SharedMemory::NULLHandle(),
              base::TimeDelta(),
              base::Closure());
      media::FillYUV(frame, 0, 128, 128);
    }
  }

  return output_buffer;
}

VideoCaptureController::~VideoCaptureController() {
  STLDeleteContainerPointers(controller_clients_.begin(),
                             controller_clients_.end());
}

void VideoCaptureController::DoIncomingCapturedI420BufferOnIOThread(
    scoped_refptr<media::VideoCaptureDevice::Client::Buffer> buffer,
    const gfx::Size& dimensions,
    int frame_rate,
    base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_NE(buffer->id(), VideoCaptureBufferPool::kInvalidId);

  VideoCaptureFormat frame_format(
      dimensions, frame_rate, media::PIXEL_FORMAT_I420);

  int count = 0;
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); ++client_it) {
      ControllerClient* client = *client_it;
      if (client->session_closed)
        continue;

      bool is_new_buffer = client->known_buffers.insert(buffer->id()).second;
      if (is_new_buffer) {
        // On the first use of a buffer on a client, share the memory handle.
        size_t memory_size = 0;
        base::SharedMemoryHandle remote_handle = buffer_pool_->ShareToProcess(
            buffer->id(), client->render_process_handle, &memory_size);
        client->event_handler->OnBufferCreated(
            client->controller_id, remote_handle, memory_size, buffer->id());
      }

      client->event_handler->OnBufferReady(
          client->controller_id, buffer->id(), timestamp, frame_format);
      bool inserted = client->active_buffers.insert(buffer->id()).second;
      DCHECK(inserted) << "Unexpected duplicate buffer: " << buffer->id();
      count++;
    }
  }

  buffer_pool_->HoldForConsumers(buffer->id(), count);
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
    if ((*client_it)->session_id == session_id) {
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
