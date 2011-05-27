// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl.h"

#include "content/common/child_process.h"
#include "content/common/video_capture_messages.h"

VideoCaptureImpl::DIBBuffer::DIBBuffer(
    TransportDIB* d, media::VideoCapture::VideoFrameBuffer* ptr)
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

VideoCaptureImpl::~VideoCaptureImpl() {}

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
    message_filter_->Send(new VideoCaptureHostMsg_Stop(0, device_id_));

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

  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::StartCapture, handler,
                          capability));
    return;
  }

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
      DLOG(INFO) << "StartCapture: Got master client with new resolution "
          "during started, try to restart.";
      StopDevice();
    }
    handler->OnStarted(this);
    return;
  }

  if (state_ == kStopping) {
    if (capability.resolution_fixed || !pending_start()) {
      new_width_ = capability.width;
      new_height_ = capability.height;
      DLOG(INFO) << "StartCapture: Got new resolution, already in stopping.";
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

  StartCaptureInternal();
}

void VideoCaptureImpl::StopCapture(media::VideoCapture::EventHandler* handler) {
  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::StopCapture, handler));
    return;
  }

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
        DLOG(INFO) << "StopCapture: New smaller resolution, stopping ...";
        StopDevice();
      }
      return;
    }

    if (state_ == kStopping) {
      new_width_ = maxw;
      new_height_ = maxh;
      DLOG(INFO) << "StopCapture: New resolution, during stopping.";
      return;
    }
  } else {
    new_width_ = width_ = 0;
    new_height_ = height_ = 0;
    DLOG(INFO) << "StopCapture: No more client, stopping ...";
    StopDevice();
  }
}

void VideoCaptureImpl::OnBufferReceived(TransportDIB::Handle handle,
                                        base::Time timestamp) {
  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::OnBufferReceived,
                          handle, timestamp));
    return;
  }

  if (state_ != kStarted) {
    message_filter_->Send(
        new VideoCaptureHostMsg_BufferReady(0, device_id_, handle));
    return;
  }

  media::VideoCapture::VideoFrameBuffer* buffer;
  CachedDIB::iterator it;
  for (it = cached_dibs_.begin(); it != cached_dibs_.end(); it++) {
    if ((*it)->dib->handle() == handle)
      break;
  }
  if (it == cached_dibs_.end()) {
    TransportDIB* dib = TransportDIB::Map(handle);
    buffer = new VideoFrameBuffer();
    buffer->memory_pointer = dib->memory();
    buffer->buffer_size = dib->size();
    buffer->width = width_;
    buffer->height = height_;

    DIBBuffer* dib_buffer = new DIBBuffer(dib, buffer);
    cached_dibs_.push_back(dib_buffer);
  } else {
    buffer = (*it)->mapped_memory;
  }

  // TODO(wjia): handle buffer sharing with downstream modules.
  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnBufferReady(this, buffer);
  }

  message_filter_->Send(
      new VideoCaptureHostMsg_BufferReady(0, device_id_, handle));
}

void VideoCaptureImpl::OnStateChanged(
    const media::VideoCapture::State& state) {
  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::OnStateChanged, state));
    return;
  }

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

void VideoCaptureImpl::OnDeviceInfoReceived(
    const media::VideoCaptureParams& device_info) {
  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::OnDeviceInfoReceived,
                          device_info));
    return;
  }

  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnDeviceInfoReceived(this, device_info);
  }
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  if (!ml_proxy_->BelongsToCurrentThread()) {
    ml_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &VideoCaptureImpl::OnDelegateAdded, device_id));
    return;
  }

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
    message_filter_->Send(new VideoCaptureHostMsg_Stop(0, device_id_));
    width_ = height_ = 0;
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
  params.session_id = session_id_;

  message_filter_->Send(new VideoCaptureHostMsg_Start(0, device_id_, params));
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
