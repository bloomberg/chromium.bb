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

void ResetBufferYV12(uint8* buffer, int width, int height) {
  int y_size = width * height;
  memset(buffer, 0, y_size);
  buffer += y_size;
  memset(buffer, 128, y_size / 2);
}

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

// The number of DIBs VideoCaptureController allocate.
static const size_t kNoOfDIBS = 3;

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

  // Handle to the render process that will receive the DIBs.
  base::ProcessHandle render_process_handle;
  media::VideoCaptureParams parameters;

  // Buffers used by this client.
  std::set<int> buffers;

  // State of capture session, controlled by VideoCaptureManager directly.
  bool session_closed;
};

struct VideoCaptureController::SharedDIB {
  SharedDIB(base::SharedMemory* ptr)
      : shared_memory(ptr),
        rotation(0),
        references(0) {
  }

  ~SharedDIB() {}

  // The memory created to be shared with renderer processes.
  scoped_ptr<base::SharedMemory> shared_memory;

  int rotation;

  // Number of renderer processes which hold this shared memory.
  // renderer process is represented by VidoeCaptureHost.
  int references;
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
  // In case capture has been started, need to check different condtions.
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
      int buffer_size = (frame_info_.width * frame_info_.height * 3) / 2;
      SendFrameInfoAndBuffers(client, buffer_size);
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
    DIBMap::iterator dib_it = owned_dibs_.find(buffer_id);
    if (dib_it == owned_dibs_.end())
      continue;

    {
      base::AutoLock lock(lock_);
      DCHECK_GT(dib_it->second->references, 0)
          << "The buffer is not used by renderer.";
      dib_it->second->references -= 1;
    }
  }
  client->buffers.clear();

  int session_id = client->parameters.session_id;
  delete client;
  controller_clients_.remove(client);

  // No more clients. Stop device.
  if (controller_clients_.empty() && state_ == VIDEO_CAPTURE_STATE_STARTED) {
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
  DIBMap::iterator dib_it = owned_dibs_.find(buffer_id);
  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  if (!client ||
      client->buffers.find(buffer_id) == client->buffers.end() ||
      dib_it == owned_dibs_.end())
    return;

  client->buffers.erase(buffer_id);
  {
    base::AutoLock lock(lock_);
    DCHECK_GT(dib_it->second->references, 0)
        << "The buffer is not used by renderer.";
    dib_it->second->references -= 1;
    if (dib_it->second->references > 0)
      return;
  }

  // When all buffers have been returned by clients and device has been
  // called to stop, check if restart is needed. This could happen when
  // capture needs to be restarted due to resolution change.
  if (!ClientHasDIB() && state_ == VIDEO_CAPTURE_STATE_STOPPING) {
    PostStopping();
  }
}

bool VideoCaptureController::ReserveSharedMemory(int* buffer_id_out,
                                                 uint8** yplane,
                                                 uint8** uplane,
                                                 uint8** vplane,
                                                 int rotation) {
  int buffer_id = 0;
  base::SharedMemory* dib = NULL;
  {
    base::AutoLock lock(lock_);
    for (DIBMap::iterator dib_it = owned_dibs_.begin();
         dib_it != owned_dibs_.end(); dib_it++) {
      if (dib_it->second->references == 0) {
        buffer_id = dib_it->first;
        // Use special value "-1" in order to not be treated as buffer at
        // renderer side.
        dib_it->second->references = -1;
        dib = dib_it->second->shared_memory.get();
        if (rotation != dib_it->second->rotation) {
          ResetBufferYV12(static_cast<uint8*>(dib->memory()),
                          frame_info_.width, frame_info_.height);
          dib_it->second->rotation = rotation;
        }
        break;
      }
    }
  }

  if (!dib)
    return false;

  *buffer_id_out = buffer_id;
  CHECK_GE(dib->created_size(),
           static_cast<size_t>(frame_info_.width * frame_info_.height * 3) / 2);
  uint8* target = static_cast<uint8*>(dib->memory());
  *yplane = target;
  *uplane = *yplane + frame_info_.width * frame_info_.height;
  *vplane = *uplane + (frame_info_.width * frame_info_.height) / 4;
  return true;
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

  int buffer_id = 0;
  uint8* yplane = NULL;
  uint8* uplane = NULL;
  uint8* vplane = NULL;
  if (!ReserveSharedMemory(&buffer_id, &yplane, &uplane, &vplane, rotation))
    return;

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
                 this, buffer_id, timestamp));
}

// OnIncomingCapturedVideoFrame is called the thread running the capture device.
void VideoCaptureController::OnIncomingCapturedVideoFrame(
    media::VideoFrame* frame,
    base::Time timestamp) {
  // Validate the inputs.
  gfx::Size target_size = gfx::Size(frame_info_.width, frame_info_.height);
  if (frame->coded_size() != target_size)
    return;  // Only exact copies are supported.
  if (!(frame->format() == media::VideoFrame::I420 ||
        frame->format() == media::VideoFrame::YV12 ||
        frame->format() == media::VideoFrame::RGB32)) {
    NOTREACHED() << "Unsupported format passed to OnIncomingCapturedVideoFrame";
    return;
  }

  // Carve out a shared memory buffer.
  int buffer_id = 0;
  uint8* yplane = NULL;
  uint8* uplane = NULL;
  uint8* vplane = NULL;
  if (!ReserveSharedMemory(&buffer_id, &yplane, &uplane, &vplane, 0))
    return;

  scoped_refptr<media::VideoFrame> target_as_frame(
      media::VideoFrame::WrapExternalYuvData(
          media::VideoFrame::YV12,  // Actually I420, but it's equivalent here.
          target_size, gfx::Rect(target_size), target_size,
          frame_info_.width,        // y stride
          frame_info_.width / 2,    // v stride
          frame_info_.width / 2,    // u stride
          yplane,
          uplane,
          vplane,
          base::TimeDelta(),
          base::Bind(&base::DoNothing)));

  const int kYPlane = media::VideoFrame::kYPlane;
  const int kUPlane = media::VideoFrame::kUPlane;
  const int kVPlane = media::VideoFrame::kVPlane;
  const int kRGBPlane = media::VideoFrame::kRGBPlane;

  // Do color conversion from the camera format to I420.
  switch (frame->format()) {
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
                        target_as_frame);
      media::CopyUPlane(frame->data(kUPlane),
                        frame->stride(kUPlane),
                        frame->rows(kUPlane),
                        target_as_frame);
      media::CopyVPlane(frame->data(kVPlane),
                        frame->stride(kVPlane),
                        frame->rows(kVPlane),
                        target_as_frame);
      break;
    }
    case media::VideoFrame::RGB32: {
      media::ConvertRGB32ToYUV(frame->data(kRGBPlane),
                               target_as_frame->data(kYPlane),
                               target_as_frame->data(kUPlane),
                               target_as_frame->data(kVPlane),
                               target_size.width(),
                               target_size.height(),
                               frame->stride(kRGBPlane),
                               target_as_frame->stride(kYPlane),
                               target_as_frame->stride(kUPlane));
      break;
    }
  }

  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoIncomingCapturedFrameOnIOThread,
                 this, buffer_id, timestamp));
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
  // Delete all DIBs.
  STLDeleteContainerPairSecondPointers(owned_dibs_.begin(),
                                       owned_dibs_.end());
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
    int buffer_id, base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int count = 0;
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); client_it++) {
      if ((*client_it)->session_closed)
        continue;

      (*client_it)->event_handler->OnBufferReady((*client_it)->controller_id,
                                                 buffer_id, timestamp);
      (*client_it)->buffers.insert(buffer_id);
      count++;
    }
  }

  base::AutoLock lock(lock_);
  if (owned_dibs_.find(buffer_id) != owned_dibs_.end()) {
    DCHECK_EQ(owned_dibs_[buffer_id]->references, -1);
    owned_dibs_[buffer_id]->references = count;
  }
}

void VideoCaptureController::DoFrameInfoOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(owned_dibs_.empty())
      << "Device is restarted without releasing shared memory.";

  // Allocate memory only when device has been started.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED)
    return;

  bool frames_created = true;
  const size_t needed_size = (frame_info_.width * frame_info_.height * 3) / 2;
  {
    base::AutoLock lock(lock_);
    for (size_t i = 1; i <= kNoOfDIBS; ++i) {
      scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());
      if (!shared_memory->CreateAndMapAnonymous(needed_size)) {
        frames_created = false;
        break;
      }
      ResetBufferYV12(static_cast<uint8*>(shared_memory->memory()),
                      frame_info_.width, frame_info_.height);
      SharedDIB* dib = new SharedDIB(shared_memory.release());
      owned_dibs_.insert(std::make_pair(i, dib));
    }
  }
  // Check whether all DIBs were created successfully.
  if (!frames_created) {
    state_ = VIDEO_CAPTURE_STATE_ERROR;
    for (ControllerClients::iterator client_it = controller_clients_.begin();
         client_it != controller_clients_.end(); client_it++) {
      (*client_it)->event_handler->OnError((*client_it)->controller_id);
    }
    return;
  }
  frame_info_available_ = true;

  for (ControllerClients::iterator client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); client_it++) {
    SendFrameInfoAndBuffers((*client_it), static_cast<int>(needed_size));
  }
}

void VideoCaptureController::DoErrorOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  state_ = VIDEO_CAPTURE_STATE_ERROR;
  ControllerClients::iterator client_it;
  for (client_it = controller_clients_.begin();
       client_it != controller_clients_.end(); client_it++) {
    (*client_it)->event_handler->OnError((*client_it)->controller_id);
  }
  for (client_it = pending_clients_.begin();
       client_it != pending_clients_.end(); client_it++) {
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

void VideoCaptureController::SendFrameInfoAndBuffers(
    ControllerClient* client, int buffer_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(frame_info_available_);
  client->event_handler->OnFrameInfo(client->controller_id,
                                     frame_info_.width, frame_info_.height,
                                     frame_info_.frame_rate);
  for (DIBMap::iterator dib_it = owned_dibs_.begin();
       dib_it != owned_dibs_.end(); dib_it++) {
    base::SharedMemory* shared_memory = dib_it->second->shared_memory.get();
    int index = dib_it->first;
    base::SharedMemoryHandle remote_handle;
    shared_memory->ShareToProcess(client->render_process_handle,
                                  &remote_handle);
    client->event_handler->OnBufferCreated(client->controller_id,
                                           remote_handle,
                                           buffer_size,
                                           index);
  }
}

VideoCaptureController::ControllerClient*
VideoCaptureController::FindClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* handler,
    const ControllerClients& clients) {
  for (ControllerClients::const_iterator client_it = clients.begin();
       client_it != clients.end(); client_it++) {
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
       client_it != clients.end(); client_it++) {
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
  if (ClientHasDIB() || device_in_use_)
    return;

  {
    base::AutoLock lock(lock_);
    STLDeleteValues(&owned_dibs_);
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
       client_it != controller_clients_.end(); client_it++) {
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

bool VideoCaptureController::ClientHasDIB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::AutoLock lock(lock_);
  for (DIBMap::iterator dib_it = owned_dibs_.begin();
       dib_it != owned_dibs_.end(); dib_it++) {
    if (dib_it->second->references > 0)
      return true;
  }
  return false;
}

}  // namespace content
