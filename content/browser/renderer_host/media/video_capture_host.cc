// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/common/media/video_capture_messages.h"

VideoCaptureHost::VideoCaptureHost() {}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close all requested VideCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); it++) {
    VideoCaptureController* controller = it->second;
    // Since the channel is closing we need a task to make sure VideoCaptureHost
    // is not deleted before VideoCaptureController.
    controller->StopCapture(
        NewRunnableMethod(this, &VideoCaptureHost::OnReadyToDelete, it->first));
  }
}

void VideoCaptureHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////

// Implements VideoCaptureControllerEventHandler.
void VideoCaptureHost::OnError(const VideoCaptureControllerID& id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureHost::DoHandleError, id.device_id));
}

void VideoCaptureHost::OnBufferCreated(
    const VideoCaptureControllerID& id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureHost::DoSendNewBuffer,
                        id.device_id, handle, length, buffer_id));
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& id,
    int buffer_id,
    base::Time timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureHost::DoSendFilledBuffer,
                        id.device_id, buffer_id, timestamp));
}

void VideoCaptureHost::OnFrameInfo(const VideoCaptureControllerID& id,
                                   int width,
                                   int height,
                                   int frame_per_second) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureHost::DoSendFrameInfo,
                        id.device_id, width, height, frame_per_second));
}

void VideoCaptureHost::OnReadyToDelete(const VideoCaptureControllerID& id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &VideoCaptureHost::DoDeleteVideoCaptureController,
                        id));
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
    controller->StopCapture(NULL);
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

  scoped_refptr<VideoCaptureController> controller =
      new VideoCaptureController(controller_id, peer_handle(), this);
  entries_.insert(std::make_pair(controller_id, controller));
  controller->StartCapture(params);
}

void VideoCaptureHost::OnStopCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    scoped_refptr<VideoCaptureController> controller = it->second;
    controller->StopCapture(NULL);
  } else {
    // It does not exist so it must have been stopped already.
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          media::VideoCapture::kStopped));
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
    controller->ReturnBuffer(buffer_id);
  }
}

void VideoCaptureHost::DoDeleteVideoCaptureController(
    const VideoCaptureControllerID& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Report that the device have successfully been stopped.
  Send(new VideoCaptureMsg_StateChanged(id.device_id,
                                        media::VideoCapture::kStopped));
  entries_.erase(id);
}
