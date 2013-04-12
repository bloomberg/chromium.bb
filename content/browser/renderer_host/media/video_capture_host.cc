// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/video_capture_messages.h"

namespace content {

struct VideoCaptureHost::Entry {
  Entry(VideoCaptureController* controller)
      : controller(controller) {}

  ~Entry() {}

  scoped_refptr<VideoCaptureController> controller;
};

VideoCaptureHost::VideoCaptureHost() {}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close all requested VideCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); it++) {
    VideoCaptureController* controller = it->second->controller;
    if (controller) {
      VideoCaptureControllerID controller_id(it->first);
      controller->StopCapture(controller_id, this);
      GetVideoCaptureManager()->RemoveController(controller, this);
    }
  }
  STLDeleteValues(&entries_);
}

void VideoCaptureHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////

// Implements VideoCaptureControllerEventHandler.
void VideoCaptureHost::OnError(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnError";
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoHandleErrorOnIOThread,
                 this, controller_id));
}

void VideoCaptureHost::OnBufferCreated(
    const VideoCaptureControllerID& controller_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendNewBufferOnIOThread,
                 this, controller_id, handle, length, buffer_id));
}

void VideoCaptureHost::OnBufferReady(
    const VideoCaptureControllerID& controller_id,
    int buffer_id,
    base::Time timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFilledBufferOnIOThread,
                 this, controller_id, buffer_id, timestamp));
}

void VideoCaptureHost::OnFrameInfo(
    const VideoCaptureControllerID& controller_id,
    int width,
    int height,
    int frame_per_second) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoSendFrameInfoOnIOThread,
                 this, controller_id, width, height, frame_per_second));
}

void VideoCaptureHost::OnEnded(const VideoCaptureControllerID& controller_id) {
  DVLOG(1) << "VideoCaptureHost::OnEnded";
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoEndedOnIOThread, this, controller_id));
}

void VideoCaptureHost::DoSendNewBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    base::SharedMemoryHandle handle,
    int length,
    int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_NewBuffer(controller_id.device_id, handle,
                                     length, buffer_id));
}

void VideoCaptureHost::DoSendFilledBufferOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int buffer_id, base::Time timestamp) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_BufferReady(controller_id.device_id, buffer_id,
                                       timestamp));
}

void VideoCaptureHost::DoHandleErrorOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_ERROR));
  DeleteVideoCaptureControllerOnIOThread(controller_id);
}

void VideoCaptureHost::DoEndedOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::DoEndedOnIOThread";
  if (entries_.find(controller_id) == entries_.end())
    return;

  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_ENDED));
  DeleteVideoCaptureControllerOnIOThread(controller_id);
}

void VideoCaptureHost::DoSendFrameInfoOnIOThread(
    const VideoCaptureControllerID& controller_id,
    int width, int height, int frame_per_second) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entries_.find(controller_id) == entries_.end())
    return;

  media::VideoCaptureParams params;
  params.width = width;
  params.height = height;
  params.frame_per_second = frame_per_second;
  Send(new VideoCaptureMsg_DeviceInfo(controller_id.device_id, params));
  Send(new VideoCaptureMsg_StateChanged(controller_id.device_id,
                                        VIDEO_CAPTURE_STATE_STARTED));
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
  DVLOG(1) << "VideoCaptureHost::OnStartCapture, device_id " << device_id
           << ", (" << params.width
           << ", " << params.height
           << ", " << params.frame_per_second
           << ", " << params.session_id
           << ")";
  VideoCaptureControllerID controller_id(device_id);
  DCHECK(entries_.find(controller_id) == entries_.end());

  entries_[controller_id] = new Entry(NULL);
  GetVideoCaptureManager()->AddController(
      params, this, base::Bind(&VideoCaptureHost::OnControllerAdded, this,
                               device_id, params));
}

void VideoCaptureHost::OnControllerAdded(
    int device_id, const media::VideoCaptureParams& params,
    VideoCaptureController* controller) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoCaptureHost::DoControllerAddedOnIOThread,
                 this, device_id, params, make_scoped_refptr(controller)));
}

void VideoCaptureHost::DoControllerAddedOnIOThread(
    int device_id, const media::VideoCaptureParams params,
    VideoCaptureController* controller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end()) {
    if (controller)
      GetVideoCaptureManager()->RemoveController(controller, this);
    return;
  }

  if (controller == NULL) {
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          VIDEO_CAPTURE_STATE_ERROR));
    delete it->second;
    entries_.erase(controller_id);
    return;
  }

  it->second->controller = controller;
  controller->StartCapture(controller_id, this, peer_handle(), params);
}

void VideoCaptureHost::OnStopCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::OnStopCapture, device_id " << device_id;

  VideoCaptureControllerID controller_id(device_id);

  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        VIDEO_CAPTURE_STATE_STOPPED));
  DeleteVideoCaptureControllerOnIOThread(controller_id);
}

void VideoCaptureHost::OnPauseCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::OnPauseCapture, device_id " << device_id;
  // Not used.
  Send(new VideoCaptureMsg_StateChanged(device_id, VIDEO_CAPTURE_STATE_ERROR));
}

void VideoCaptureHost::OnReceiveEmptyBuffer(int device_id, int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    scoped_refptr<VideoCaptureController> controller = it->second->controller;
    if (controller)
      controller->ReturnBuffer(controller_id, this, buffer_id);
  }
}

void VideoCaptureHost::DeleteVideoCaptureControllerOnIOThread(
    const VideoCaptureControllerID& controller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  EntryMap::iterator it = entries_.find(controller_id);
  if (it == entries_.end())
    return;

  VideoCaptureController* controller = it->second->controller;
  if (controller) {
    controller->StopCapture(controller_id, this);
    GetVideoCaptureManager()->RemoveController(controller, this);
  }
  delete it->second;
  entries_.erase(controller_id);
}

VideoCaptureManager* VideoCaptureHost::GetVideoCaptureManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return BrowserMainLoop::GetMediaStreamManager()->video_capture_manager();
}

}  // namespace content
