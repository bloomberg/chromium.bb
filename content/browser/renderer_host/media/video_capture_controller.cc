// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <set>

#include "base/bind.h"
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

  // State of capture session, controlled by VideoCaptureManager directly.
  bool session_closed;
};

VideoCaptureController::VideoCaptureController(
    VideoCaptureManager* video_capture_manager)
    : chopped_width_(0),
      chopped_height_(0),
      frame_info_available_(false),
      video_capture_manager_(video_capture_manager),
      device_in_use_(false),
      state_(VIDEO_CAPTURE_STATE_STOPPED) {
  memset(&current_params_, 0, sizeof(current_params_));
}

void VideoCaptureController::StartCapture(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    base::ProcessHandle render_process,
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::StartCapture, id " << id.device_id
           << ", (" << params.width
           << ", " << params.height
           << ", " << params.frame_per_second
           << ", " << params.session_id
           << ")";

  // Signal error in case device is already in error state.
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(id);
    return;
  }

  // Do nothing if this client has called StartCapture before.
  if (FindClient(id, event_handler, controller_clients_) ||
      FindClient(id, event_handler, pending_clients_))
    return;

  ControllerClient* client = new ControllerClient(id, event_handler,
                                                  render_process, params);
  // In case capture has been started, need to check different conditions.
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
  // TODO(wjia): Temporarily disable restarting till client supports resampling.
#if 0
    // This client has higher resolution than what is currently requested.
    // Need restart capturing.
    if (params.width > current_params_.width ||
        params.height > current_params_.height) {
      video_capture_manager_->Stop(current_params_.session_id,
          base::Bind(&VideoCaptureController::OnDeviceStopped, this));
      frame_info_available_ = false;
      state_ = VIDEO_CAPTURE_STATE_STOPPING;
      pending_clients_.push_back(client);
      return;
    }
#endif

    // This client's resolution is no larger than what's currently requested.
    // When frame_info has been returned by device, send them to client.
    if (frame_info_available_) {
      SendFrameInfoAndBuffers(client);
    }
    controller_clients_.push_back(client);
    return;
  }

  // In case the device is in the middle of stopping, put the client in
  // pending queue.
  if (state_ == VIDEO_CAPTURE_STATE_STOPPING) {
    pending_clients_.push_back(client);
    return;
  }

  // Fresh start.
  controller_clients_.push_back(client);
  current_params_ = params;
  // Order the manager to start the actual capture.
  video_capture_manager_->Start(params, this);
  state_ = VIDEO_CAPTURE_STATE_STARTED;
  device_in_use_ = true;
}

void VideoCaptureController::StopCapture(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::StopCapture, id " << id.device_id;

  ControllerClient* client = FindClient(id, event_handler, pending_clients_);
  // If the client is still in pending queue, just remove it.
  if (client) {
    pending_clients_.remove(client);
    return;
  }

  client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return;

  // Take back all buffers held by the |client|.
  for (std::set<int>::iterator buffer_it = client->buffers.begin();
       buffer_it != client->buffers.end(); ++buffer_it) {
    int buffer_id = *buffer_it;
    buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
  }
  client->buffers.clear();

  int session_id = client->parameters.session_id;
  delete client;
  controller_clients_.remove(client);

  // No more clients. Stop device.
  if (controller_clients_.empty() &&
      (state_ == VIDEO_CAPTURE_STATE_STARTED ||
       state_ == VIDEO_CAPTURE_STATE_ERROR)) {
    video_capture_manager_->Stop(session_id,
        base::Bind(&VideoCaptureController::OnDeviceStopped, this));
    frame_info_available_ = false;
    state_ = VIDEO_CAPTURE_STATE_STOPPING;
  }
}

void VideoCaptureController::StopSession(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureController::StopSession, id " << session_id;

  ControllerClient* client = FindClient(session_id, pending_clients_);
  if (!client)
    client = FindClient(session_id, controller_clients_);

  if (client) {
    client->session_closed = true;
    client->event_handler->OnPaused(client->controller_id);
  }
}

void VideoCaptureController::ReturnBuffer(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ControllerClient* client = FindClient(id, event_handler,
                                        controller_clients_);

  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  if (!client ||
      client->buffers.find(buffer_id) == client->buffers.end())
    return;

  client->buffers.erase(buffer_id);
  buffer_pool_->RelinquishConsumerHold(buffer_id, 1);

  // When all buffers have been returned by clients and device has been
  // called to stop, check if restart is needed. This could happen when
  // capture needs to be restarted due to resolution change.
  if (!buffer_pool_->IsAnyBufferHeldForConsumers() &&
      state_ == VIDEO_CAPTURE_STATE_STOPPING) {
    PostStopping();
  }
}

scoped_refptr<media::VideoFrame> VideoCaptureController::ReserveOutputBuffer() {
  base::AutoLock lock(buffer_pool_lock_);
  if (!buffer_pool_)
    return NULL;
  return buffer_pool_->ReserveForProducer(0);
}

// Implements VideoCaptureDevice::EventHandler.
// OnIncomingCapturedFrame is called the thread running the capture device.
// I.e.- DirectShow thread on windows and v4l2_thread on Linux.
void VideoCaptureController::OnIncomingCapturedFrame(
    const uint8* data,
    int length,
    base::Time timestamp,
    int rotation,
    bool flip_vert,
    bool flip_horiz) {
  DCHECK (frame_info_.color == media::VideoCaptureCapability::kI420 ||
          frame_info_.color == media::VideoCaptureCapability::kYV12 ||
          (rotation == 0 && !flip_vert && !flip_horiz));

  scoped_refptr<media::VideoFrame> dst;
  {
    base::AutoLock lock(buffer_pool_lock_);
    if (!buffer_pool_)
      return;
    dst = buffer_pool_->ReserveForProducer(rotation);
  }

  if (!dst)
    return;

  uint8* yplane = dst->data(media::VideoFrame::kYPlane);
  uint8* uplane = dst->data(media::VideoFrame::kUPlane);
  uint8* vplane = dst->data(media::VideoFrame::kVPlane);

  // Do color conversion from the camera format to I420.
  switch (frame_info_.color) {
    case media::VideoCaptureCapability::kColorUnknown:  // Color format not set.
      break;
    case media::VideoCaptureCapability::kI420:
      DCHECK(!chopped_width_ && !chopped_height_);
      RotatePackedYV12Frame(
          data, yplane, uplane, vplane, frame_info_.width, frame_info_.height,
          rotation, flip_vert, flip_horiz);
      break;
    case media::VideoCaptureCapability::kYV12:
      DCHECK(!chopped_width_ && !chopped_height_);
      RotatePackedYV12Frame(
          data, yplane, vplane, uplane, frame_info_.width, frame_info_.height,
          rotation, flip_vert, flip_horiz);
      break;
    case media::VideoCaptureCapability::kNV21:
      DCHECK(!chopped_width_ && !chopped_height_);
      media::ConvertNV21ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                              frame_info_.height);
      break;
    case media::VideoCaptureCapability::kYUY2:
      DCHECK(!chopped_width_ && !chopped_height_);
      if (frame_info_.width * frame_info_.height * 2 != length) {
        // If |length| of |data| does not match the expected width and height
        // we can't convert the frame to I420. YUY2 is 2 bytes per pixel.
        break;
      }

      media::ConvertYUY2ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                              frame_info_.height);
      break;
    case media::VideoCaptureCapability::kRGB24: {
      int ystride = frame_info_.width;
      int uvstride = frame_info_.width / 2;
#if defined(OS_WIN)  // RGB on Windows start at the bottom line.
      int rgb_stride = -3 * (frame_info_.width + chopped_width_);
      const uint8* rgb_src = data + 3 * (frame_info_.width + chopped_width_) *
          (frame_info_.height -1 + chopped_height_);
#else
      int rgb_stride = 3 * (frame_info_.width + chopped_width_);
      const uint8* rgb_src = data;
#endif
      media::ConvertRGB24ToYUV(rgb_src, yplane, uplane, vplane,
                               frame_info_.width, frame_info_.height,
                               rgb_stride, ystride, uvstride);
      break;
    }
    case media::VideoCaptureCapability::kARGB:
      media::ConvertRGB32ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                               frame_info_.height,
                               (frame_info_.width + chopped_width_) * 4,
                               frame_info_.width, frame_info_.width / 2);
      break;
#if !defined(OS_IOS) && !defined(OS_ANDROID)
    case media::VideoCaptureCapability::kMJPEG: {
      int yplane_stride = frame_info_.width;
      int uv_plane_stride = (frame_info_.width + 1) / 2;
      int crop_x = 0;
      int crop_y = 0;
      libyuv::ConvertToI420(data, length, yplane, yplane_stride, uplane,
                            uv_plane_stride, vplane, uv_plane_stride, crop_x,
                            crop_y, frame_info_.width, frame_info_.height,
                            frame_info_.width, frame_info_.height,
                            libyuv::kRotate0, libyuv::FOURCC_MJPG);
      break;
    }
#endif
    default:
      NOTREACHED();
  }

  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                 this, dst, timestamp));
}

// OnIncomingCapturedVideoFrame is called the thread running the capture device.
void VideoCaptureController::OnIncomingCapturedVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    base::Time timestamp) {

  scoped_refptr<media::VideoFrame> target;
  {
    base::AutoLock lock(buffer_pool_lock_);

    if (!buffer_pool_)
      return;

    // If this is a frame that belongs to the buffer pool, we can forward it
    // directly to the IO thread and be done.
    if (buffer_pool_->RecognizeReservedBuffer(frame)) {
      BrowserThread::PostTask(BrowserThread::IO,
          FROM_HERE,
          base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                     this, frame, timestamp));
      return;
    }
    // Otherwise, this is a frame that belongs to the caller, and we must copy
    // it to a frame from the buffer pool.
    target = buffer_pool_->ReserveForProducer(0);
  }

  if (!target)
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
                        target);
      media::CopyUPlane(frame->data(kUPlane),
                        frame->stride(kUPlane),
                        frame->rows(kUPlane),
                        target);
      media::CopyVPlane(frame->data(kVPlane),
                        frame->stride(kVPlane),
                        frame->rows(kVPlane),
                        target);
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
                 this, target, timestamp));
}

void VideoCaptureController::OnError() {
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, this));
}

void VideoCaptureController::OnFrameInfo(
    const media::VideoCaptureCapability& info) {
  frame_info_= info;
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
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoFrameInfoOnIOThread, this));
}

VideoCaptureController::~VideoCaptureController() {
  buffer_pool_ = NULL;  // Release all buffers.
  STLDeleteContainerPointers(controller_clients_.begin(),
                             controller_clients_.end());
  STLDeleteContainerPointers(pending_clients_.begin(),
                             pending_clients_.end());
}

// Called by VideoCaptureManager when a device have been stopped.
void VideoCaptureController::OnDeviceStopped() {
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoDeviceStoppedOnIOThread, this));
}

void VideoCaptureController::DoIncomingCapturedFrameOnIOThread(
    const scoped_refptr<media::VideoFrame>& reserved_frame,
    base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!buffer_pool_)
    return;

  int buffer_id = buffer_pool_->RecognizeReservedBuffer(reserved_frame);
  if (!buffer_id) {
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

  buffer_pool_->HoldForConsumers(reserved_frame, buffer_id, count);
}

void VideoCaptureController::DoFrameInfoOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!buffer_pool_)
      << "Device is restarted without releasing shared memory.";

  // Allocate memory only when device has been started.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;

  scoped_refptr<VideoCaptureBufferPool> buffer_pool =
      new VideoCaptureBufferPool(
          gfx::Size(frame_info_.width, frame_info_.height), kNoOfBuffers);

  // Check whether all buffers were created successfully.
  if (!buffer_pool->Allocate()) {
    state_ = VIDEO_CAPTURE_STATE_ERROR;
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); ++client_it) {
      (*client_it)->event_handler->OnError((*client_it)->controller_id);
    }
    return;
  }

  {
    base::AutoLock lock(buffer_pool_lock_);
    buffer_pool_ = buffer_pool;
  }
  frame_info_available_ = true;

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    SendFrameInfoAndBuffers(*client_it);
  }
}

void VideoCaptureController::DoErrorOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state_ = VIDEO_CAPTURE_STATE_ERROR;
  ControllerClients::iterator client_it;
  for (client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    (*client_it)->event_handler->OnError((*client_it)->controller_id);
  }
  for (client_it = pending_clients_.begin();
       client_it != pending_clients_.end(); ++client_it) {
    (*client_it)->event_handler->OnError((*client_it)->controller_id);
  }
}

void VideoCaptureController::DoDeviceStoppedOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  device_in_use_ = false;
  if (state_ == VIDEO_CAPTURE_STATE_STOPPING) {
    PostStopping();
  }
}

void VideoCaptureController::SendFrameInfoAndBuffers(ControllerClient* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(frame_info_available_);
  client->event_handler->OnFrameInfo(client->controller_id,
                                     frame_info_.width, frame_info_.height,
                                     frame_info_.frame_rate);
  for (int buffer_id = 1; buffer_id <= buffer_pool_->count(); ++buffer_id) {
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

// This function is called when all buffers have been returned to controller,
// or when device is stopped. It decides whether the device needs to be
// restarted.
void VideoCaptureController::PostStopping() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPING);

  // When clients still have some buffers, or device has not been stopped yet,
  // do nothing.
  if (buffer_pool_->IsAnyBufferHeldForConsumers() || device_in_use_)
    return;

  {
    base::AutoLock lock(buffer_pool_lock_);
    buffer_pool_ = NULL;
  }

  // No more client. Therefore the controller is stopped.
  if (controller_clients_.empty() && pending_clients_.empty()) {
    state_ = VIDEO_CAPTURE_STATE_STOPPED;
    return;
  }

  // Restart the device.
  current_params_.width = 0;
  current_params_.height = 0;
  ControllerClients::iterator client_it;
  for (client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); ++client_it) {
    if (current_params_.width < (*client_it)->parameters.width)
      current_params_.width = (*client_it)->parameters.width;
    if (current_params_.height < (*client_it)->parameters.height)
      current_params_.height = (*client_it)->parameters.height;
  }
  for (client_it = pending_clients_.begin();
       client_it != pending_clients_.end(); ) {
    if (current_params_.width < (*client_it)->parameters.width)
      current_params_.width = (*client_it)->parameters.width;
    if (current_params_.height < (*client_it)->parameters.height)
      current_params_.height = (*client_it)->parameters.height;
    controller_clients_.push_back((*client_it));
    pending_clients_.erase(client_it++);
  }
  // Request the manager to start the actual capture.
  video_capture_manager_->Start(current_params_, this);
  state_ = VIDEO_CAPTURE_STATE_STARTED;
  device_in_use_ = true;
}

}  // namespace content
