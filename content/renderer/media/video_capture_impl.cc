// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture_messages.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

namespace content {

class VideoCaptureImpl::ClientBuffer
    : public base::RefCountedThreadSafe<ClientBuffer> {
 public:
  ClientBuffer(scoped_ptr<base::SharedMemory> buffer,
               size_t buffer_size)
      : buffer(buffer.Pass()),
        buffer_size(buffer_size) {}
  const scoped_ptr<base::SharedMemory> buffer;
  const size_t buffer_size;

 private:
  friend class base::RefCountedThreadSafe<ClientBuffer>;

  virtual ~ClientBuffer() {}

  DISALLOW_COPY_AND_ASSIGN(ClientBuffer);
};

bool VideoCaptureImpl::CaptureStarted() {
  return state_ == VIDEO_CAPTURE_STATE_STARTED;
}

int VideoCaptureImpl::CaptureFrameRate() {
  return last_frame_format_.frame_rate;
}

VideoCaptureImpl::VideoCaptureImpl(
    const media::VideoCaptureSessionId session_id,
    VideoCaptureMessageFilter* filter)
    : VideoCapture(),
      message_filter_(filter),
      io_message_loop_proxy_(ChildProcess::current()->io_message_loop_proxy()),
      device_id_(0),
      session_id_(session_id),
      suspended_(false),
      state_(VIDEO_CAPTURE_STATE_STOPPED),
      weak_this_factory_(this) {
  DCHECK(filter);
}

VideoCaptureImpl::~VideoCaptureImpl() {}

void VideoCaptureImpl::Init() {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::InitOnIOThread,
                 base::Unretained(this)));
}

void VideoCaptureImpl::DeInit(base::Closure done_cb) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DeInitOnIOThread,
                 base::Unretained(this),
                 done_cb));
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::SuspendCaptureOnIOThread,
                 base::Unretained(this),
                 suspend));
}

void VideoCaptureImpl::StartCapture(
    media::VideoCapture::EventHandler* handler,
    const media::VideoCaptureParams& params) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::StartCaptureOnIOThread,
                 base::Unretained(this), handler, params));
}

void VideoCaptureImpl::StopCapture(
    media::VideoCapture::EventHandler* handler) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::StopCaptureOnIOThread,
                 base::Unretained(this), handler));
}

void VideoCaptureImpl::GetDeviceSupportedFormats(
    const DeviceFormatsCallback& callback) {
  DCHECK(!callback.is_null());
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::GetDeviceSupportedFormatsOnIOThread,
                 base::Unretained(this), media::BindToCurrentLoop(callback)));
}

void VideoCaptureImpl::GetDeviceFormatsInUse(
    const DeviceFormatsInUseCallback& callback) {
  DCHECK(!callback.is_null());
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::GetDeviceFormatsInUseOnIOThread,
                 base::Unretained(this), media::BindToCurrentLoop(callback)));
}

void VideoCaptureImpl::InitOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  message_filter_->AddDelegate(this);
}

void VideoCaptureImpl::DeInitOnIOThread(base::Closure done_cb) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == VIDEO_CAPTURE_STATE_STARTED)
    Send(new VideoCaptureHostMsg_Stop(device_id_));
  message_filter_->RemoveDelegate(this);
  done_cb.Run();
}

void VideoCaptureImpl::SuspendCaptureOnIOThread(bool suspend) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  suspended_ = suspend;
}

void VideoCaptureImpl::StartCaptureOnIOThread(
    media::VideoCapture::EventHandler* handler,
    const media::VideoCaptureParams& params) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    handler->OnError(this, 1);
    handler->OnRemoved(this);
  } else if ((clients_pending_on_filter_.find(handler) !=
              clients_pending_on_filter_.end()) ||
             (clients_pending_on_restart_.find(handler) !=
              clients_pending_on_restart_.end()) ||
             clients_.find(handler) != clients_.end() ) {
    // This client has started.
  } else if (!device_id_) {
    clients_pending_on_filter_[handler] = params;
  } else {
    handler->OnStarted(this);
    if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
      clients_[handler] = params;
    } else if (state_ == VIDEO_CAPTURE_STATE_STOPPING) {
      clients_pending_on_restart_[handler] = params;
      DVLOG(1) << "StartCapture: Got new resolution "
               << params.requested_format.frame_size.ToString()
               << " during stopping.";
    } else {
      // TODO(sheu): Allowing resolution change will require that all
      // outstanding clients of a capture session support resolution change.
      DCHECK(!params.allow_resolution_change);
      clients_[handler] = params;
      DCHECK_EQ(1ul, clients_.size());
      params_ = params;
      if (params_.requested_format.frame_rate >
          media::limits::kMaxFramesPerSecond) {
        params_.requested_format.frame_rate =
            media::limits::kMaxFramesPerSecond;
      }
      DVLOG(1) << "StartCapture: starting with first resolution "
               << params_.requested_format.frame_size.ToString();
      first_frame_timestamp_ = base::TimeTicks();
      StartCaptureInternal();
    }
  }
}

void VideoCaptureImpl::StopCaptureOnIOThread(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // A handler can be in only one client list.
  // If this handler is in any client list, we can just remove it from
  // that client list and don't have to run the other following RemoveClient().
  RemoveClient(handler, &clients_pending_on_filter_) ||
  RemoveClient(handler, &clients_pending_on_restart_) ||
  RemoveClient(handler, &clients_);

  if (clients_.empty()) {
    DVLOG(1) << "StopCapture: No more client, stopping ...";
    StopDevice();
    client_buffers_.clear();
    weak_this_factory_.InvalidateWeakPtrs();
  }
}

void VideoCaptureImpl::GetDeviceSupportedFormatsOnIOThread(
    const DeviceFormatsCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  device_formats_callback_queue_.push_back(callback);
  if (device_formats_callback_queue_.size() == 1)
    Send(new VideoCaptureHostMsg_GetDeviceSupportedFormats(device_id_,
                                                           session_id_));
}

void VideoCaptureImpl::GetDeviceFormatsInUseOnIOThread(
    const DeviceFormatsInUseCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  device_formats_in_use_callback_queue_.push_back(callback);
  if (device_formats_in_use_callback_queue_.size() == 1)
    Send(
        new VideoCaptureHostMsg_GetDeviceFormatsInUse(device_id_, session_id_));
}

void VideoCaptureImpl::OnBufferCreated(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    base::SharedMemory::CloseHandle(handle);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory(handle, false));
  if (!shm->Map(length)) {
    DLOG(ERROR) << "OnBufferCreated: Map failed.";
    return;
  }

  bool inserted =
      client_buffers_.insert(std::make_pair(
                                 buffer_id,
                                 new ClientBuffer(shm.Pass(),
                                                  length))).second;
  DCHECK(inserted);
}

void VideoCaptureImpl::OnBufferDestroyed(int buffer_id) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  ClientBufferMap::iterator iter = client_buffers_.find(buffer_id);
  if (iter == client_buffers_.end())
    return;

  DCHECK(!iter->second || iter->second->HasOneRef())
      << "Instructed to delete buffer we are still using.";
  client_buffers_.erase(iter);
}

void VideoCaptureImpl::OnBufferReceived(
    int buffer_id,
    base::TimeTicks timestamp,
    const media::VideoCaptureFormat& format) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (state_ != VIDEO_CAPTURE_STATE_STARTED || suspended_) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id));
    return;
  }

  last_frame_format_ = format;
  if (first_frame_timestamp_.is_null())
    first_frame_timestamp_ = timestamp;

  ClientBufferMap::iterator iter = client_buffers_.find(buffer_id);
  DCHECK(iter != client_buffers_.end());
  scoped_refptr<ClientBuffer> buffer = iter->second;
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalPackedMemory(
          media::VideoFrame::I420,
          last_frame_format_.frame_size,
          gfx::Rect(last_frame_format_.frame_size),
          last_frame_format_.frame_size,
          reinterpret_cast<uint8*>(buffer->buffer->memory()),
          buffer->buffer_size,
          buffer->buffer->handle(),
          timestamp - first_frame_timestamp_,
          media::BindToCurrentLoop(
              base::Bind(
                  &VideoCaptureImpl::OnClientBufferFinished,
                  weak_this_factory_.GetWeakPtr(),
                  buffer_id,
                  buffer)));

  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); ++it)
    it->first->OnFrameReady(this, frame);
}

void VideoCaptureImpl::OnClientBufferFinished(
    int buffer_id,
    const scoped_refptr<ClientBuffer>& buffer) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id));
}

void VideoCaptureImpl::OnStateChanged(VideoCaptureState state) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  switch (state) {
    case VIDEO_CAPTURE_STATE_STARTED:
      break;
    case VIDEO_CAPTURE_STATE_STOPPED:
      state_ = VIDEO_CAPTURE_STATE_STOPPED;
      DVLOG(1) << "OnStateChanged: stopped!, device_id = " << device_id_;
      client_buffers_.clear();
      weak_this_factory_.InvalidateWeakPtrs();
      if (!clients_.empty() || !clients_pending_on_restart_.empty())
        RestartCapture();
      break;
    case VIDEO_CAPTURE_STATE_PAUSED:
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); ++it) {
        it->first->OnPaused(this);
      }
      break;
    case VIDEO_CAPTURE_STATE_ERROR:
      DVLOG(1) << "OnStateChanged: error!, device_id = " << device_id_;
      for (ClientInfo::iterator it = clients_.begin();
           it != clients_.end(); ++it) {
        // TODO(wjia): browser process would send error code.
        it->first->OnError(this, 1);
        it->first->OnRemoved(this);
      }
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ERROR;
      break;
    case VIDEO_CAPTURE_STATE_ENDED:
      DVLOG(1) << "OnStateChanged: ended!, device_id = " << device_id_;
      for (ClientInfo::iterator it = clients_.begin();
          it != clients_.end(); ++it) {
        it->first->OnRemoved(this);
      }
      clients_.clear();
      state_ = VIDEO_CAPTURE_STATE_ENDED;
      break;
    default:
      break;
  }
}

void VideoCaptureImpl::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& supported_formats) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_callback_queue_.size(); ++i)
    device_formats_callback_queue_[i].Run(supported_formats);
  device_formats_callback_queue_.clear();
}

void VideoCaptureImpl::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  for (size_t i = 0; i < device_formats_in_use_callback_queue_.size(); ++i)
    device_formats_in_use_callback_queue_[i].Run(formats_in_use);
  device_formats_in_use_callback_queue_.clear();
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  DVLOG(1) << "OnDelegateAdded: device_id " << device_id;
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  device_id_ = device_id;
  for (ClientInfo::iterator it = clients_pending_on_filter_.begin();
       it != clients_pending_on_filter_.end(); ) {
    media::VideoCapture::EventHandler* handler = it->first;
    const media::VideoCaptureParams params = it->second;
    clients_pending_on_filter_.erase(it++);
    StartCapture(handler, params);
  }
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    state_ = VIDEO_CAPTURE_STATE_STOPPING;
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    params_.requested_format.frame_size.SetSize(0, 0);
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);

  int width = 0;
  int height = 0;
  for (ClientInfo::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    width = std::max(width, it->second.requested_format.frame_size.width());
    height = std::max(height, it->second.requested_format.frame_size.height());
  }
  for (ClientInfo::iterator it = clients_pending_on_restart_.begin();
       it != clients_pending_on_restart_.end(); ) {
    width = std::max(width, it->second.requested_format.frame_size.width());
    height = std::max(height, it->second.requested_format.frame_size.height());
    clients_[it->first] = it->second;
    clients_pending_on_restart_.erase(it++);
  }
  params_.requested_format.frame_size.SetSize(width, height);
  DVLOG(1) << "RestartCapture, "
           << params_.requested_format.frame_size.ToString();
  StartCaptureInternal();
}

void VideoCaptureImpl::StartCaptureInternal() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(device_id_);

  Send(new VideoCaptureHostMsg_Start(device_id_, session_id_, params_));
  state_ = VIDEO_CAPTURE_STATE_STARTED;
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(base::IgnoreResult(&VideoCaptureMessageFilter::Send),
                 message_filter_.get(), message));
}

bool VideoCaptureImpl::RemoveClient(
    media::VideoCapture::EventHandler* handler,
    ClientInfo* clients) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  bool found = false;

  ClientInfo::iterator it = clients->find(handler);
  if (it != clients->end()) {
    handler->OnStopped(this);
    handler->OnRemoved(this);
    clients->erase(it);
    found = true;
  }
  return found;
}

}  // namespace content
