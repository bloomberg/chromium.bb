// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl.h"

#include "base/stl_util-inl.h"
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
  return width_;
}

int VideoCaptureImpl::CaptureHeight() {
  return height_;
}

int VideoCaptureImpl::CaptureFrameRate() {
  return frame_rate_;
}

VideoCaptureImpl::VideoCaptureImpl(
    const media::VideoCaptureSessionId id,
    scoped_refptr<base::MessageLoopProxy> ml_proxy,
    VideoCaptureMessageFilter* filter)
    : VideoCapture(),
      message_filter_(filter),
      session_id_(id),
      ml_proxy_(ml_proxy),
      device_id_(0),
      width_(0),
      height_(0),
      frame_rate_(0),
      video_type_(media::VideoFrame::I420),
      new_width_(0),
      new_height_(0),
      state_(kStopped) {
  DCHECK(filter);
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
    return;
  }

  if (!device_id_) {
    pending_clients_[handler] = capability;
    return;
  }

  if (capability.resolution_fixed && master_clients_.size() &&
      (capability.width != width_ || capability.height != height_)) {
    // Can't have 2 master clients with different resolutions.
    handler->OnError(this, 1);
    return;
  }

  clients_[handler] = capability;
  if (capability.resolution_fixed) {
    master_clients_.push_back(handler);
    if (master_clients_.size() > 1)
      return;
  }

  if (state_ == kStarted) {
    // Take the resolution of master client.
    if (capability.resolution_fixed &&
        (capability.width != width_ || capability.height != height_)) {
      new_width_ = capability.width;
      new_height_ = capability.height;
      DLOG(INFO) << "StartCapture: Got master client with new resolution ("
                 << new_width_ << ", " << new_height_ << ") "
                 << "during started, try to restart.";
      StopDevice();
    }
    handler->OnStarted(this);
    return;
  }

  if (state_ == kStopping) {
    if (capability.resolution_fixed || !pending_start()) {
      new_width_ = capability.width;
      new_height_ = capability.height;
      DLOG(INFO) << "StartCapture: Got new resolution ("
                 << new_width_ << ", " << new_height_ << ") "
                 << ", already in stopping.";
    }
    handler->OnStarted(this);
    return;
  }

  DCHECK_EQ(clients_.size(), 1ul);
  video_type_ = capability.raw_type;
  new_width_ = 0;
  new_height_ = 0;
  width_ = capability.width;
  height_ = capability.height;
  DLOG(INFO) << "StartCapture: resolution ("
             << width_ << ", " << height_ << "). ";

  StartCaptureInternal();
}

void VideoCaptureImpl::DoStopCapture(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  ClientInfo::iterator it = pending_clients_.find(handler);
  if (it != pending_clients_.end()) {
    handler->OnStopped(this);
    pending_clients_.erase(it);
    return;
  }

  if (clients_.find(handler) == clients_.end())
    return;

  handler->OnStopped(this);
  clients_.erase(handler);
  master_clients_.remove(handler);

  // Still have at least one master client.
  if (master_clients_.size() > 0)
    return;

  // TODO(wjia): Is it really needed to handle resolution change for non-master
  // clients, except no client case?
  if (clients_.size() > 0) {
    DLOG(INFO) << "StopCapture: No master client.";
    int maxw = 0;
    int maxh = 0;
    for (ClientInfo::iterator it = clients_.begin();
         it != clients_.end(); it++) {
      if (it->second.width > maxw && it->second.height > maxh) {
        maxw = it->second.width;
        maxh = it->second.height;
      }
    }

    if (state_ == kStarted) {
      // Only handle resolution reduction.
      if (maxw < width_ && maxh < height_) {
        new_width_ = maxw;
        new_height_ = maxh;
        DLOG(INFO) << "StopCapture: New smaller resolution ("
                   << new_width_ << ", " << new_height_ << ") "
                   << "), stopping ...";
        StopDevice();
      }
      return;
    }

    if (state_ == kStopping) {
      new_width_ = maxw;
      new_height_ = maxh;
      DLOG(INFO) << "StopCapture: New resolution ("
                 << new_width_ << ", " << new_height_ << ") "
                 << "), during stopping.";
      return;
    }
  } else {
    new_width_ = width_ = 0;
    new_height_ = height_ = 0;
    DLOG(INFO) << "StopCapture: No more client, stopping ...";
    StopDevice();
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
  buffer->memory_pointer = dib->memory();
  buffer->buffer_size = length;
  buffer->width = width_;
  buffer->height = height_;

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

  // TODO(wjia): handle buffer sharing with downstream modules.
  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnBufferReady(this, buffer);
  }

  Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id));
}

void VideoCaptureImpl::DoStateChanged(const media::VideoCapture::State& state) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  switch (state) {
    case media::VideoCapture::kStarted:
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); it++) {
        it->first->OnStarted(this);
      }
      break;
    case media::VideoCapture::kStopped:
      state_ = kStopped;
      DLOG(INFO) << "OnStateChanged: stopped!, device_id = " << device_id_;
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
      }
      break;
    default:
      break;
  }
}

void VideoCaptureImpl::DoDeviceInfoReceived(
    const media::VideoCaptureParams& device_info) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

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
  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::StopDevice));
    return;
  }

  if (state_ == kStarted) {
    state_ = kStopping;
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    width_ = height_ = 0;
    STLDeleteContainerPairSecondPointers(cached_dibs_.begin(),
                                         cached_dibs_.end());
    cached_dibs_.clear();
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopped);

  width_ = new_width_;
  height_ = new_height_;
  new_width_ = 0;
  new_height_ = 0;

  DLOG(INFO) << "RestartCapture, " << width_ << ", " << height_;
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK(device_id_);

  media::VideoCaptureParams params;
  params.width = width_;
  params.height = height_;
  params.frame_per_second = frame_rate_;
  params.session_id = session_id_;

  Send(new VideoCaptureHostMsg_Start(device_id_, params));
  state_ = kStarted;
  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnStarted(this);
  }
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
