// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/common/child_process.h"
#include "content/common/media/video_capture_messages.h"

struct VideoCaptureImpl::DIBBuffer {
 public:
  DIBBuffer(
      base::SharedMemory* d,
      media::VideoCapture::VideoFrameBuffer* ptr)
      : dib(d),
        mapped_memory(ptr),
        references(0) {
  }
  ~DIBBuffer() {}

  scoped_ptr<base::SharedMemory> dib;
  scoped_refptr<media::VideoCapture::VideoFrameBuffer> mapped_memory;

  // Number of clients which hold this DIB.
  int references;
};

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
  current_params_.session_id = id;
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
        base::Bind(&VideoCaptureImpl::AddDelegateOnIOThread,
                   base::Unretained(this)));
    return;
  }

  AddDelegateOnIOThread();
}

void VideoCaptureImpl::DeInit(base::Closure task) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoDeInit,
                 base::Unretained(this), task));
}

void VideoCaptureImpl::DoDeInit(base::Closure task) {
  if (state_ == kStarted)
    Send(new VideoCaptureHostMsg_Stop(device_id_));

  base::MessageLoopProxy* io_message_loop_proxy =
      ChildProcess::current()->io_message_loop_proxy();

  io_message_loop_proxy->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::RemoveDelegateOnIOThread,
                 base::Unretained(this), task));
}

void VideoCaptureImpl::StartCapture(
    media::VideoCapture::EventHandler* handler,
    const VideoCaptureCapability& capability) {
  DCHECK_EQ(capability.raw_type, media::VideoFrame::I420);

  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoStartCapture,
                 base::Unretained(this), handler, capability));
}

void VideoCaptureImpl::StopCapture(media::VideoCapture::EventHandler* handler) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoStopCapture,
                 base::Unretained(this), handler));
}

void VideoCaptureImpl::FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoFeedBuffer,
                 base::Unretained(this), buffer));
}

void VideoCaptureImpl::OnBufferCreated(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoBufferCreated,
                 base::Unretained(this), handle, length, buffer_id));
}

void VideoCaptureImpl::OnBufferReceived(int buffer_id, base::Time timestamp) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoBufferReceived,
                 base::Unretained(this), buffer_id, timestamp));
}

void VideoCaptureImpl::OnStateChanged(const media::VideoCapture::State& state) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoStateChanged,
                 base::Unretained(this), state));
}

void VideoCaptureImpl::OnDeviceInfoReceived(
    const media::VideoCaptureParams& device_info) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoDeviceInfoReceived,
                 base::Unretained(this), device_info));
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  ml_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoDelegateAdded,
                 base::Unretained(this), device_id));
}

void VideoCaptureImpl::DoStartCapture(
    media::VideoCapture::EventHandler* handler,
    const VideoCaptureCapability& capability) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  if (state_ == kError) {
    handler->OnError(this, 1);
    handler->OnRemoved(this);
    return;
  }

  ClientInfo::iterator it1 = clients_pending_on_filter_.find(handler);
  ClientInfo::iterator it2 = clients_pending_on_restart_.find(handler);
  if (it1 != clients_pending_on_filter_.end() ||
      it2 != clients_pending_on_restart_.end() ||
      clients_.find(handler) != clients_.end() ) {
    // This client has started.
    return;
  }

  if (!device_id_) {
    clients_pending_on_filter_[handler] = capability;
    return;
  }

  handler->OnStarted(this);
  if (state_ == kStarted) {
    if (capability.width > current_params_.width ||
        capability.height > current_params_.height) {
      StopDevice();
      DVLOG(1) << "StartCapture: Got client with higher resolution ("
               << capability.width << ", " << capability.height << ") "
               << "after started, try to restart.";
      clients_pending_on_restart_[handler] = capability;
      return;
    }

    if (device_info_available_) {
      handler->OnDeviceInfoReceived(this, device_info_);
    }

    clients_[handler] = capability;
    return;
  }

  if (state_ == kStopping) {
    clients_pending_on_restart_[handler] = capability;
    DVLOG(1) << "StartCapture: Got new resolution ("
             << capability.width << ", " << capability.height << ") "
             << ", during stopping.";
    return;
  }

  clients_[handler] = capability;
  DCHECK_EQ(clients_.size(), 1ul);
  video_type_ = capability.raw_type;
  current_params_.width = capability.width;
  current_params_.height = capability.height;
  current_params_.frame_per_second = capability.max_fps;
  DVLOG(1) << "StartCapture: starting with first resolution ("
           << current_params_.width << ", " << current_params_.height << ")";

  StartCaptureInternal();
}

void VideoCaptureImpl::DoStopCapture(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  ClientInfo::iterator it = clients_pending_on_filter_.find(handler);
  if (it != clients_pending_on_filter_.end()) {
    handler->OnStopped(this);
    handler->OnRemoved(this);
    clients_pending_on_filter_.erase(it);
    return;
  }
  it = clients_pending_on_restart_.find(handler);
  if (it != clients_pending_on_restart_.end()) {
    handler->OnStopped(this);
    handler->OnRemoved(this);
    clients_pending_on_filter_.erase(it);
    return;
  }

  if (clients_.find(handler) == clients_.end())
    return;

  clients_.erase(handler);

  if (clients_.empty()) {
    DVLOG(1) << "StopCapture: No more client, stopping ...";
    StopDevice();
  }
  handler->OnStopped(this);
  handler->OnRemoved(this);
}

void VideoCaptureImpl::DoFeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  CachedDIB::iterator it;
  for (it = cached_dibs_.begin(); it != cached_dibs_.end(); it++) {
    if (buffer == it->second->mapped_memory)
      break;
  }

  DCHECK(it != cached_dibs_.end());
  DCHECK_GT(it->second->references, 0);
  it->second->references--;
  if (it->second->references == 0) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, it->first));
  }
}

void VideoCaptureImpl::DoBufferCreated(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK(device_info_available_);

  media::VideoCapture::VideoFrameBuffer* buffer;
  DCHECK(cached_dibs_.find(buffer_id) == cached_dibs_.end());

  base::SharedMemory* dib = new base::SharedMemory(handle, false);
  dib->Map(length);
  buffer = new VideoFrameBuffer();
  buffer->memory_pointer = static_cast<uint8*>(dib->memory());
  buffer->buffer_size = length;
  buffer->width = device_info_.width;
  buffer->height = device_info_.height;
  buffer->stride = device_info_.width;

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
  cached_dibs_[buffer_id]->references = clients_.size();
}

void VideoCaptureImpl::DoStateChanged(const media::VideoCapture::State& state) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  switch (state) {
    case media::VideoCapture::kStarted:
      break;
    case media::VideoCapture::kStopped:
      state_ = kStopped;
      DVLOG(1) << "OnStateChanged: stopped!, device_id = " << device_id_;
      STLDeleteValues(&cached_dibs_);
      if (!clients_.empty() || !clients_pending_on_restart_.empty())
        RestartCapture();
      break;
    case media::VideoCapture::kPaused:
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); it++) {
        it->first->OnPaused(this);
      }
      break;
    case media::VideoCapture::kError:
      DVLOG(1) << "OnStateChanged: error!, device_id = " << device_id_;
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); it++) {
        // TODO(wjia): browser process would send error code.
        it->first->OnError(this, 1);
        it->first->OnRemoved(this);
      }
      clients_.clear();
      state_ = kError;
      break;
    default:
      break;
  }
}

void VideoCaptureImpl::DoDeviceInfoReceived(
    const media::VideoCaptureParams& device_info) {
  DCHECK(ml_proxy_->BelongsToCurrentThread());
  DCHECK(!ClientHasDIB());

  STLDeleteValues(&cached_dibs_);

  device_info_ = device_info;
  device_info_available_ = true;
  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); it++) {
    it->first->OnDeviceInfoReceived(this, device_info);
  }
}

void VideoCaptureImpl::DoDelegateAdded(int32 device_id) {
  DVLOG(1) << "DoDelegateAdded: device_id " << device_id;
  DCHECK(ml_proxy_->BelongsToCurrentThread());

  device_id_ = device_id;
  for (ClientInfo::iterator it = clients_pending_on_filter_.begin();
       it != clients_pending_on_filter_.end(); ) {
    media::VideoCapture::EventHandler* handler = it->first;
    const VideoCaptureCapability capability = it->second;
    clients_pending_on_filter_.erase(it++);
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

  int width = 0;
  int height = 0;
  for (ClientInfo::iterator it = clients_.begin();
       it != clients_.end(); it++) {
    if (it->second.width > width)
      width = it->second.width;
    if (it->second.height > height)
      height = it->second.height;
  }
  for (ClientInfo::iterator it = clients_pending_on_restart_.begin();
       it != clients_pending_on_restart_.end(); ) {
    if (it->second.width > width)
      width = it->second.width;
    if (it->second.height > height)
      height = it->second.height;
    clients_[it->first] = it->second;
    clients_pending_on_restart_.erase(it++);
  }
  current_params_.width = width;
  current_params_.height = height;
  DVLOG(1) << "RestartCapture, " << current_params_.width << ", "
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

void VideoCaptureImpl::RemoveDelegateOnIOThread(base::Closure task) {
  message_filter_->RemoveDelegate(this);
  ml_proxy_->PostTask(FROM_HERE, task);
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  base::MessageLoopProxy* io_message_loop_proxy =
      ChildProcess::current()->io_message_loop_proxy();

  io_message_loop_proxy->PostTask(FROM_HERE,
      base::IgnoreReturn<bool>(base::Bind(&VideoCaptureMessageFilter::Send,
                                          message_filter_.get(), message)));
}

bool VideoCaptureImpl::ClientHasDIB() {
  CachedDIB::iterator it;
  for (it = cached_dibs_.begin(); it != cached_dibs_.end(); it++) {
    if (it->second->references > 0)
      return true;
  }
  return false;
}
