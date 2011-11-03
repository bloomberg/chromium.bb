// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/resource_context.h"
#include "content/common/media/video_capture_messages.h"

using content::BrowserThread;

VideoCaptureHost::VideoCaptureHost(
    const content::ResourceContext* resource_context)
    : resource_context_(resource_context) {
}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close all requested VideCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); it++) {
    VideoCaptureController* controller = it->second;
    VideoCaptureControllerID controller_id(it->first);
    controller->StopCapture(controller_id, this, true);
    GetVideoCaptureManager()->RemoveController(controller, this);
  }
  entries_.clear();
  entry_state_.clear();
}

void VideoCaptureHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////

// Implements VideoCaptureControllerEventHandler.
void VideoCaptureHost::OnError(const VideoCaptureControllerID& id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoHandleError, this, id.device_id));
}

void VideoCaptureHost::OnBufferCreated(
    const VideoCaptureControllerID& id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendNewBuffer,
                 this, id.device_id, handle, length, buffer_id));
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& id,
    int buffer_id,
    base::Time timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFilledBuffer,
                 this, id.device_id, buffer_id, timestamp));
}

void VideoCaptureHost::OnFrameInfo(const VideoCaptureControllerID& id,
                                   int width,
                                   int height,
                                   int frame_per_second) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFrameInfo,
                 this, id.device_id, width, height, frame_per_second));
}

void VideoCaptureHost::OnReadyToDelete(const VideoCaptureControllerID& id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoDeleteVideoCaptureController, this, id));
}

void VideoCaptureHost::DoSendNewBuffer(
    int device_id, base::SharedMemoryHandle handle,
    int length, int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  Send(new VideoCaptureMsg_NewBuffer(device_id, handle,
                                     length, buffer_id));
}

void VideoCaptureHost::DoSendFilledBuffer(
    int device_id, int buffer_id, base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  Send(new VideoCaptureMsg_BufferReady(device_id, buffer_id,
                                       timestamp));
}

void VideoCaptureHost::DoHandleError(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        media::VideoCapture::kError));

  VideoCaptureControllerID id(device_id);
  EntryMap::iterator it = entries_.find(id);
  if (it != entries_.end()) {
    VideoCaptureController* controller = it->second;
    controller->StopCapture(id, this, false);
  }
}

void VideoCaptureHost::DoSendFrameInfo(int device_id,
                                       int width,
                                       int height,
                                       int frame_per_second) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  media::VideoCaptureParams params;
  params.width = width;
  params.height = height;
  params.frame_per_second = frame_per_second;
  Send(new VideoCaptureMsg_DeviceInfo(device_id, params));
  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        media::VideoCapture::kStarted));
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler.
bool VideoCaptureHost::OnMessageReceived(const IPC::Message& message,
                                         bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(VideoCaptureHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, OnStartCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, OnPauseCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, OnStopCapture)
    IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady, OnReceiveEmptyBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void VideoCaptureHost::OnStartCapture(int device_id,
                                      const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VideoCaptureControllerID controller_id(device_id);
  DCHECK(entries_.find(controller_id) == entries_.end());
  DCHECK(entry_state_.find(controller_id) == entry_state_.end());

  entry_state_[controller_id] = media::VideoCapture::kStarted;
  GetVideoCaptureManager()->AddController(
      params, this, base::Bind(&VideoCaptureHost::OnControllerAdded, this,
                               device_id, params));
}

void VideoCaptureHost::OnControllerAdded(
    int device_id, const media::VideoCaptureParams& params,
    VideoCaptureController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoControllerAddedOnIOThreead,
                 this, device_id, params, make_scoped_refptr(controller)));
}

void VideoCaptureHost::DoControllerAddedOnIOThreead(
    int device_id, const media::VideoCaptureParams params,
    VideoCaptureController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VideoCaptureControllerID controller_id(device_id);
  DCHECK(entries_.find(controller_id) == entries_.end());
  DCHECK(entry_state_.find(controller_id) != entry_state_.end());

  if (controller == NULL) {
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          media::VideoCapture::kError));
    return;
  }

  entries_.insert(std::make_pair(controller_id, controller));
  if (entry_state_[controller_id] == media::VideoCapture::kStarted) {
    controller->StartCapture(controller_id, this, peer_handle(), params);
  } else if (entry_state_[controller_id] == media::VideoCapture::kStopped) {
    controller->StopCapture(controller_id, this, false);
  }
}

void VideoCaptureHost::OnStopCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VideoCaptureControllerID controller_id(device_id);

  if (entry_state_.find(controller_id) == entry_state_.end()) {
    // It does not exist. So it must have been stopped already.
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          media::VideoCapture::kStopped));
  }

  entry_state_[controller_id] = media::VideoCapture::kStopped;

  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    scoped_refptr<VideoCaptureController> controller = it->second;
    controller->StopCapture(controller_id, this, false);
  }
}

void VideoCaptureHost::OnPauseCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Not used.
  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        media::VideoCapture::kError));
}

void VideoCaptureHost::OnReceiveEmptyBuffer(int device_id, int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    scoped_refptr<VideoCaptureController> controller = it->second;
    controller->ReturnBuffer(controller_id, this, buffer_id);
  }
}

void VideoCaptureHost::DoDeleteVideoCaptureController(
    const VideoCaptureControllerID& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Report that the device have successfully been stopped.
  Send(new VideoCaptureMsg_StateChanged(id.device_id,
                                        media::VideoCapture::kStopped));
  EntryMap::iterator it = entries_.find(id);
  if (it != entries_.end()) {
    GetVideoCaptureManager()->RemoveController(it->second, this);
  }
  entries_.erase(id);
  entry_state_.erase(id);
}

media_stream::VideoCaptureManager* VideoCaptureHost::GetVideoCaptureManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return resource_context_->media_stream_manager()->video_capture_manager();
}
