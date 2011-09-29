// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl.h"

#include "base/stl_util.h"
#include "content/common/child_process.h"
#include "content/common/media/video_capture_messages.h"

VideoCaptureImpl::DIBBuffer::DIBBuffer(
    base::SharedMemory* d, media::VideoCapture::VideoFrameBuffer* ptr)
    : dib(d),
      mapped_memory(ptr) {}

VideoCaptureImpl::DIBBuffer::~DIBBuffer() {
  delete dib;
}

bool VideoCaptureImpl::CaptureStarted() {
  return state_ == kStarted;
}

int VideoCaptureImpl::CaptureWidth() {
  return current_params_.width;
}

int VideoCaptureImpl::CaptureHeight() {
  return current_params_.height;
}

int VideoCaptureImpl::CaptureFrameRate() {
  return current_params_.frame_per_second;
}

VideoCaptureImpl::VideoCaptureImpl(
    const media::VideoCaptureSessionId id,
    scoped_refptr<base::MessageLoopProxy> ml_proxy,
    VideoCaptureMessageFilter* filter)
    : VideoCapture(),
      message_filter_(filter),
      ml_proxy_(ml_proxy),
      device_id_(0),
      video_type_(media::VideoFrame::I420),
      device_info_available_(false),
      state_(kStopped) {
  DCHECK(filter);
  memset(&current_params_, 0, sizeof(current_params_));
  memset(&new_params_, 0, sizeof(new_params_));
  current_params_.session_id = new_params_.session_id = id;
}

VideoCaptureImpl::~VideoCaptureImpl() {
  STLDeleteContainerPairSecondPointers(cached_dibs_.begin(),
                                       cached_dibs_.end());
}

void VideoCaptureImpl::Init() {
  base::MessageLoopProxy* io_message_loop_proxy =
      ChildProcess::current()->io_message_loop_proxy();

  if (!io_message_loop_proxy->BelongsToCurrentThread()) {
    io_message_loop_proxy->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::AddDelegateOnIOThread));
    return;
  }

  AddDelegateOnIOThread();
}

void VideoCaptureImpl::DeInit(Task* task) {
  if (state_ == kStarted)
    Send(new VideoCaptureHostMsg_Stop(device_id_));

  base::MessageLoopProxy* io_message_loop_proxy =
      ChildProcess::current()->io_message_loop_proxy();

  if (!io_message_loop_proxy->BelongsToCurrentThread()) {
    io_message_loop_proxy->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::RemoveDelegateOnIOThread,
                          task));
    return;
  }

  RemoveDelegateOnIOThread(task);
}

void VideoCaptureImpl::StartCapture(
    media::VideoCapture::EventHandler* handler,
    const VideoCaptureCapability& capability) {
  DCHECK_EQ(capability.raw_type, media::VideoFrame::I420);

  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoStartCapture, handler,
                        capability));
}

void VideoCaptureImpl::StopCapture(media::VideoCapture::EventHandler* handler) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoStopCapture, handler));
}

void VideoCaptureImpl::FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoFeedBuffer, buffer));
}

void VideoCaptureImpl::OnBufferCreated(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoBufferCreated,
                        handle, length, buffer_id));
}

void VideoCaptureImpl::OnBufferReceived(int buffer_id, base::Time timestamp) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoBufferReceived,
                        buffer_id, timestamp));
}

void VideoCaptureImpl::OnStateChanged(const media::VideoCapture::State& state) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoStateChanged, state));
}

void VideoCaptureImpl::OnDeviceInfoReceived(
    const media::VideoCaptureParams& device_info) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoDeviceInfoReceived,
                        device_info));
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  ml_proxy_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureImpl::DoDelegateAdded, device_id));
}

void VideoCaptureImpl::DoStartCapture(
    media::VideoCapture::EventHandler* handler,
    const VideoCaptureCapability& capability) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  ClientInfo::iterator it = pending_clients_.find(handler);

  if (it != pending_clients_.end()) {
    handler->OnError(this, 1);
    handler->OnRemoved(this);
    return;
  }

  if (!device_id_) {
    pending_clients_[handler] = capability;
    return;
  }

  if (capability.resolution_fixed && master_clients_.size() &&
      (capability.width != current_params_.width ||
       capability.height != current_params_.height)) {
    // Can't have 2 master clients with different resolutions.
    handler->OnError(this, 1);
    handler->OnRemoved(this);
    return;
  }

  handler->OnStarted(this);
  clients_[handler] = capability;
  if (capability.resolution_fixed) {
    master_clients_.push_back(handler);
    if (master_clients_.size() > 1) {
      // TODO(wjia): OnStarted might be called twice if VideoCaptureImpl will
      // receive kStarted from browser process later.
      if (device_info_available_)
        handler->OnDeviceInfoReceived(this, device_info_);
      return;
    }
  }

  if (state_ == kStarted) {
    // Take the resolution of master client.
    if (capability.resolution_fixed &&
        (capability.width != current_params_.width ||
         capability.height != current_params_.height ||
         capability.max_fps != current_params_.frame_per_second)) {
      new_params_.width = capability.width;
      new_params_.height = capability.height;
      new_params_.frame_per_second = capability.max_fps;
      DLOG(INFO) << "StartCapture: Got master client with new resolution ("
                 << new_params_.width << ", " << new_params_.height << ") "
                 << "during started, try to restart.";
      StopDevice();
    } else {
      if (device_info_available_)
        handler->OnDeviceInfoReceived(this, device_info_);
    }
    return;
  }

  if (state_ == kStopping) {
    if (capability.resolution_fixed || !pending_start()) {
      new_params_.width = capability.width;
      new_params_.height = capability.height;
      new_params_.frame_per_second = capability.max_fps;
      DLOG(INFO) << "StartCapture: Got new resolution ("
                 << new_params_.width << ", " << new_params_.height << ") "
                 << ", already in stopping.";
    }
    return;
  }

  DCHECK_EQ(clients_.size(), 1ul);
  video_type_ = capability.raw_type;
  new_params_.width = 0;
  new_params_.height = 0;
  new_params_.frame_per_second = 0;
  current_params_.width = capability.width;
  current_params_.height = capability.height;
  current_params_.frame_per_second = capability.max_fps;
  DLOG(INFO) << "StartCapture: resolution ("
             << current_params_.width << ", " << current_params_.height << ")";

  StartCaptureInternal();
}

void VideoCaptureImpl::DoStopCapture(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  ClientInfo::iterator it = pending_clients_.find(handler);
  if (it != pending_clients_.end()) {
    handler->OnStopped(this);
    handler->OnRemoved(this);
    pending_clients_.erase(it);
    return;
  }

  if (clients_.find(handler) == clients_.end())
    return;

  handler->OnStopped(this);
  handler->OnRemoved(this);
  clients_.erase(handler);
  master_clients_.remove(handler);

  // Still have at least one master client.
  if (master_clients_.size() > 0)
    return;

  // TODO(wjia): Is it really needed to handle resolution change for non-master
  // clients, except no client case?
  if (clients_.size() > 0) {
    DLOG(INFO) << "StopCapture: No master client.";
    int max_width = 0;
    int max_height = 0;
    int frame_rate = 0;
    for (ClientInfo::iterator it = clients_.begin();
         it != clients_.end(); it++) {
      if (it->second.width > max_width && it->second.height > max_height) {
        max_width = it->second.width;
        max_height = it->second.height;
        frame_rate = it->second.max_fps;
      }
    }

    if (state_ == kStarted) {
      // Only handle resolution reduction.
      if (max_width < current_params_.width &&
          max_height < current_params_.height) {
        new_params_.width = max_width;
        new_params_.height = max_height;
        new_params_.frame_per_second = frame_rate;
        DLOG(INFO) << "StopCapture: New smaller resolution ("
                   << new_params_.width << ", " << new_params_.height << ") "
                   << "), stopping ...";
        StopDevice();
      }
      return;
    }

    if (state_ == kStopping) {
      new_params_.width = max_width;
      new_params_.height = max_height;
      new_params_.frame_per_second = frame_rate;
      DLOG(INFO) << "StopCapture: New resolution ("
                 << new_params_.width << ", " << new_params_.height << ") "
                 << "), during stopping.";
      return;
    }
  } else {
    new_params_.width = current_params_.width = 0;
    new_params_.height = current_params_.height = 0;
    new_params_.frame_per_second = current_params_.frame_per_second = 0;
    DLOG(INFO) << "StopCapture: No more client, stopping ...";
    StopDevice();
  }
}

void VideoCaptureImpl::DoFeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK(client_side_dibs_.find(buffer) != client_side_dibs_.end());

  CachedDIB::iterator it;
  for (it = cached_dibs_.begin(); it != cached_dibs_.end(); it++) {
    if (buffer == it->second->mapped_memory)
      break;
  }

  DCHECK(it != cached_dibs_.end());
  if (client_side_dibs_[buffer] <= 1) {
    client_side_dibs_.erase(buffer);
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, it->first));
  } else {
    client_side_dibs_[buffer]--;
  }
}

void VideoCaptureImpl::DoBufferCreated(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  media::VideoCapture::VideoFrameBuffer* buffer;
  DCHECK(cached_dibs_.find(buffer_id) == cached_dibs_.end());

  base::SharedMemory* dib = new base::SharedMemory(handle, false);
  dib->Map(length);
  buffer = new VideoFrameBuffer();
  buffer->memory_pointer = static_cast<uint8*>(dib->memory());
  buffer->buffer_size = length;
  buffer->width = current_params_.width;
  buffer->height = current_params_.height;

  DIBBuffer* dib_buffer = new DIBBuffer(dib, buffer);
  cached_dibs_[buffer_id] = dib_buffer;
}

void VideoCaptureImpl::DoBufferReceived(int buffer_id, base::Time timestamp) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  if (state_ != kStarted) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id));
    return;
  }

  media::VideoCapture::VideoFrameBuffer* buffer;
  DCHECK(cached_dibs_.find(buffer_id) != cached_dibs_.end());
  buffer = cached_dibs_[buffer_id]->mapped_memory;

  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnBufferReady(this, buffer);
  }
  client_side_dibs_[buffer] = clients_.size();
}

void VideoCaptureImpl::DoStateChanged(const media::VideoCapture::State& state) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  switch (state) {
    case media::VideoCapture::kStarted:
      break;
    case media::VideoCapture::kStopped:
      state_ = kStopped;
      DLOG(INFO) << "OnStateChanged: stopped!, device_id = " << device_id_;
      STLDeleteValues(&cached_dibs_);
      if (pending_start())
        RestartCapture();
      break;
    case media::VideoCapture::kPaused:
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); it++) {
        it->first->OnPaused(this);
      }
      break;
    case media::VideoCapture::kError:
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); it++) {
        // TODO(wjia): browser process would send error code.
        it->first->OnError(this, 1);
        it->first->OnRemoved(this);
      }
      clients_.clear();
      master_clients_.clear();
      state_ = kStopped;
      current_params_.width = current_params_.height = 0;
      break;
    default:
      break;
  }
}

void VideoCaptureImpl::DoDeviceInfoReceived(
    const media::VideoCaptureParams& device_info) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  device_info_ = device_info;
  device_info_available_ = true;
  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnDeviceInfoReceived(this, device_info);
  }
}

void VideoCaptureImpl::DoDelegateAdded(int32 device_id) {
  DLOG(INFO) << "DoDelegateAdded: device_id " << device_id;
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  device_id_ = device_id;
  for (ClientInfo::iterator it = pending_clients_.begin();
       it != pending_clients_.end(); ) {
    media::VideoCapture::EventHandler* handler = it->first;
    const VideoCaptureCapability capability = it->second;
    pending_clients_.erase(it++);
    StartCapture(handler, capability);
  }
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  device_info_available_ = false;
  if (state_ == kStarted) {
    state_ = kStopping;
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    current_params_.width = current_params_.height = 0;
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopped);

  current_params_.width = new_params_.width;
  current_params_.height = new_params_.height;
  current_params_.frame_per_second = new_params_.frame_per_second;

  new_params_.width = 0;
  new_params_.height = 0;
  new_params_.frame_per_second = 0;

  DLOG(INFO) << "RestartCapture, " << current_params_.width << ", "
             << current_params_.height;
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK(device_id_);

  Send(new VideoCaptureHostMsg_Start(device_id_, current_params_));
  state_ = kStarted;
}

void VideoCaptureImpl::AddDelegateOnIOThread() {
  message_filter_->AddDelegate(this);
}

void VideoCaptureImpl::RemoveDelegateOnIOThread(Task* task) {
  base::ScopedTaskRunner task_runner(task);
  message_filter_->RemoveDelegate(this);
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  base::MessageLoopProxy* io_message_loop_proxy =
      ChildProcess::current()->io_message_loop_proxy();

  io_message_loop_proxy->PostTask(FROM_HERE,
      NewRunnableMethod(message_filter_.get(),
                        &VideoCaptureMessageFilter::Send, message));
}
