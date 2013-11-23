// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture_messages.h"
#include "media/base/bind_to_loop.h"
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
    base::MessageLoopProxy* capture_message_loop_proxy,
    VideoCaptureMessageFilter* filter)
    : VideoCapture(),
      message_filter_(filter),
      capture_message_loop_proxy_(capture_message_loop_proxy),
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
  if (!io_message_loop_proxy_->BelongsToCurrentThread()) {
    io_message_loop_proxy_->PostTask(FROM_HERE,
        base::Bind(&VideoCaptureImpl::AddDelegateOnIOThread,
                   base::Unretained(this)));
  } else {
    AddDelegateOnIOThread();
  }
}

void VideoCaptureImpl::DeInit(base::Closure task) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoDeInitOnCaptureThread,
                 base::Unretained(this), task));
}

void VideoCaptureImpl::StartCapture(
    media::VideoCapture::EventHandler* handler,
    const media::VideoCaptureParams& params) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoStartCaptureOnCaptureThread,
                 base::Unretained(this), handler, params));
}

void VideoCaptureImpl::StopCapture(media::VideoCapture::EventHandler* handler) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoStopCaptureOnCaptureThread,
                 base::Unretained(this), handler));
}

void VideoCaptureImpl::OnBufferCreated(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoBufferCreatedOnCaptureThread,
                 base::Unretained(this), handle, length, buffer_id));
}

void VideoCaptureImpl::OnBufferDestroyed(int buffer_id) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoBufferDestroyedOnCaptureThread,
                 base::Unretained(this), buffer_id));
}

void VideoCaptureImpl::OnBufferReceived(
    int buffer_id,
    base::Time timestamp,
    const media::VideoCaptureFormat& format) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoBufferReceivedOnCaptureThread,
                 base::Unretained(this), buffer_id, timestamp, format));
}

void VideoCaptureImpl::OnStateChanged(VideoCaptureState state) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoStateChangedOnCaptureThread,
                 base::Unretained(this), state));
}

void VideoCaptureImpl::OnDelegateAdded(int32 device_id) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoDelegateAddedOnCaptureThread,
                 base::Unretained(this), device_id));
}

void VideoCaptureImpl::SuspendCapture(bool suspend) {
  capture_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::DoSuspendCaptureOnCaptureThread,
                 base::Unretained(this), suspend));
}

void VideoCaptureImpl::DoDeInitOnCaptureThread(base::Closure task) {
  if (state_ == VIDEO_CAPTURE_STATE_STARTED)
    Send(new VideoCaptureHostMsg_Stop(device_id_));

  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(&VideoCaptureImpl::RemoveDelegateOnIOThread,
                 base::Unretained(this), task));
}

void VideoCaptureImpl::DoStartCaptureOnCaptureThread(
    media::VideoCapture::EventHandler* handler,
    const media::VideoCaptureParams& params) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

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

      StartCaptureInternal();
    }
  }
}

void VideoCaptureImpl::DoStopCaptureOnCaptureThread(
    media::VideoCapture::EventHandler* handler) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

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

void VideoCaptureImpl::DoBufferCreatedOnCaptureThread(
    base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

  // In case client calls StopCapture before the arrival of created buffer,
  // just close this buffer and return.
  if (state_ != VIDEO_CAPTURE_STATE_STARTED) {
    base::SharedMemory::CloseHandle(handle);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory(handle, false));
  if (!shm->Map(length)) {
    DLOG(ERROR) << "DoBufferCreatedOnCaptureThread: Map() failed.";
    return;
  }

  bool inserted =
      client_buffers_.insert(std::make_pair(
                                 buffer_id,
                                 new ClientBuffer(shm.Pass(),
                                                  length))).second;
  DCHECK(inserted);
}

void VideoCaptureImpl::DoBufferDestroyedOnCaptureThread(int buffer_id) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

  ClientBufferMap::iterator iter = client_buffers_.find(buffer_id);
  if (iter == client_buffers_.end())
    return;

  DCHECK(!iter->second || iter->second->HasOneRef())
      << "Instructed to delete buffer we are still using.";
  client_buffers_.erase(iter);
}

void VideoCaptureImpl::DoBufferReceivedOnCaptureThread(
    int buffer_id,
    base::Time timestamp,
    const media::VideoCaptureFormat& format) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

  if (state_ != VIDEO_CAPTURE_STATE_STARTED || suspended_) {
    Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id));
    return;
  }

  last_frame_format_ = format;

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
          // TODO(sheu): convert VideoCaptureMessageFilter::Delegate to use
          // base::TimeTicks instead of base::Time.  http://crbug.com/249215
          timestamp - base::Time::UnixEpoch(),
          media::BindToLoop(
              capture_message_loop_proxy_,
              base::Bind(
                  &VideoCaptureImpl::DoClientBufferFinishedOnCaptureThread,
                  weak_this_factory_.GetWeakPtr(),
                  buffer_id,
                  buffer)));

  for (ClientInfo::iterator it = clients_.begin(); it != clients_.end(); ++it)
    it->first->OnFrameReady(this, frame);
}

void VideoCaptureImpl::DoClientBufferFinishedOnCaptureThread(
    int buffer_id,
    const scoped_refptr<ClientBuffer>& buffer) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());
  Send(new VideoCaptureHostMsg_BufferReady(device_id_, buffer_id));
}

void VideoCaptureImpl::DoStateChangedOnCaptureThread(VideoCaptureState state) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

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

void VideoCaptureImpl::DoDelegateAddedOnCaptureThread(int32 device_id) {
  DVLOG(1) << "DoDelegateAdded: device_id " << device_id;
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

  device_id_ = device_id;
  for (ClientInfo::iterator it = clients_pending_on_filter_.begin();
       it != clients_pending_on_filter_.end(); ) {
    media::VideoCapture::EventHandler* handler = it->first;
    const media::VideoCaptureParams params = it->second;
    clients_pending_on_filter_.erase(it++);
    StartCapture(handler, params);
  }
}

void VideoCaptureImpl::DoSuspendCaptureOnCaptureThread(bool suspend) {
  DVLOG(1) << "DoSuspendCapture: suspend " << (suspend ? "yes" : "no");
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

  suspended_ = suspend;
}

void VideoCaptureImpl::StopDevice() {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    state_ = VIDEO_CAPTURE_STATE_STOPPING;
    Send(new VideoCaptureHostMsg_Stop(device_id_));
    params_.requested_format.frame_size.SetSize(0, 0);
  }
}

void VideoCaptureImpl::RestartCapture() {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());
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
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(device_id_);

  Send(new VideoCaptureHostMsg_Start(device_id_, session_id_, params_));
  state_ = VIDEO_CAPTURE_STATE_STARTED;
}

void VideoCaptureImpl::AddDelegateOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  message_filter_->AddDelegate(this);
}

void VideoCaptureImpl::RemoveDelegateOnIOThread(base::Closure task) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  message_filter_->RemoveDelegate(this);
  capture_message_loop_proxy_->PostTask(FROM_HERE, task);
}

void VideoCaptureImpl::Send(IPC::Message* message) {
  io_message_loop_proxy_->PostTask(FROM_HERE,
      base::Bind(base::IgnoreResult(&VideoCaptureMessageFilter::Send),
                 message_filter_.get(), message));
}

bool VideoCaptureImpl::RemoveClient(
    media::VideoCapture::EventHandler* handler,
    ClientInfo* clients) {
  DCHECK(capture_message_loop_proxy_->BelongsToCurrentThread());
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
