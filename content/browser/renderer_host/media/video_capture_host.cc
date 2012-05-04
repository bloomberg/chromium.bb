// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_host.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/video_capture_messages.h"

using content::BrowserMessageFilter;
using content::BrowserThread;

struct VideoCaptureHost::Entry {
  Entry() : state(video_capture::kStopped) {}

  Entry(VideoCaptureController* controller, video_capture::State state)
      : controller(controller),
        state(state) {
  }

  ~Entry() {}

  scoped_refptr<VideoCaptureController> controller;

  // Record state of each VideoCaptureControllerID.
  // There are 5 states:
  // kStarting: host has requested a controller, but has not got it yet.
  // kStarted: host got a controller and has called StartCapture.
  // kStopping: renderer process requests StopCapture before host gets
  //            a controller.
  // kStopped: host has called StopCapture.
  // kError: an error occurred and capture is stopped consequently.
  video_capture::State state;
};

VideoCaptureHost::VideoCaptureHost(content::ResourceContext* resource_context,
                                   media::AudioManager* audio_manager)
    : resource_context_(resource_context),
      audio_manager_(audio_manager) {
}

VideoCaptureHost::~VideoCaptureHost() {}

void VideoCaptureHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close all requested VideCaptureDevices.
  for (EntryMap::iterator it = entries_.begin(); it != entries_.end(); it++) {
    VideoCaptureController* controller = it->second->controller;
    if (controller) {
      VideoCaptureControllerID controller_id(it->first);
      controller->StopCapture(controller_id, this, true);
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

  VideoCaptureControllerID id(device_id);
  EntryMap::iterator it = entries_.find(id);
  if (it == entries_.end() || it->second->state == video_capture::kError)
    return;

  it->second->state = video_capture::kError;
  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        video_capture::kError));

  VideoCaptureController* controller = it->second->controller;
  controller->StopCapture(id, this, false);
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
                                        video_capture::kStarted));
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

  entries_[controller_id] = new Entry(NULL, video_capture::kStarting);
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
  DCHECK(it != entries_.end());

  if (controller == NULL) {
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          video_capture::kError));
    return;
  }

  Entry* entry = it->second;
  entry->controller = controller;
  if (entry->state == video_capture::kStarting) {
    entry->state = video_capture::kStarted;
    controller->StartCapture(controller_id, this, peer_handle(), params);
  } else if (entry->state == video_capture::kStopping) {
    DoDeleteVideoCaptureController(controller_id);
  } else {
    NOTREACHED();
  }
}

void VideoCaptureHost::OnStopCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "VideoCaptureHost::OnStopCapture, device_id " << device_id;

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);

  if (it == entries_.end()) {
    // It does not exist. So it must have been stopped already.
    Send(new VideoCaptureMsg_StateChanged(device_id,
                                          video_capture::kStopped));
    return;
  }

  Entry* entry = it->second;
  if (entry->state == video_capture::kStopping ||
      entry->state == video_capture::kStopped ||
      entry->state == video_capture::kError) {
    return;
  }

  if (entry->controller) {
    entry->state = video_capture::kStopped;
    scoped_refptr<VideoCaptureController> controller = entry->controller;
    controller->StopCapture(controller_id, this, false);
  } else {
    entry->state = video_capture::kStopping;
  }
}

void VideoCaptureHost::OnPauseCapture(int device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Not used.
  Send(new VideoCaptureMsg_StateChanged(device_id,
                                        video_capture::kError));
}

void VideoCaptureHost::OnReceiveEmptyBuffer(int device_id, int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  VideoCaptureControllerID controller_id(device_id);
  EntryMap::iterator it = entries_.find(controller_id);
  if (it != entries_.end()) {
    scoped_refptr<VideoCaptureController> controller = it->second->controller;
    controller->ReturnBuffer(controller_id, this, buffer_id);
  }
}

void VideoCaptureHost::DoDeleteVideoCaptureController(
    const VideoCaptureControllerID& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Report that the device have successfully been stopped.
  Send(new VideoCaptureMsg_StateChanged(id.device_id,
                                        video_capture::kStopped));
  EntryMap::iterator it = entries_.find(id);
  if (it != entries_.end()) {
    if (it->second->controller) {
      GetVideoCaptureManager()->RemoveController(it->second->controller, this);
    }
    delete it->second;
    entries_.erase(id);
  }
}

media_stream::VideoCaptureManager* VideoCaptureHost::GetVideoCaptureManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return media_stream::MediaStreamManager::GetForResourceContext(
      resource_context_, audio_manager_)->video_capture_manager();
}
