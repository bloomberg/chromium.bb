// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureHost serves video capture related messages from
// VideCaptureMessageFilter which lives inside the render process.
//
// This class is owned by BrowserRenderProcessHost, and instantiated on UI
// thread, but all other operations and method calls happen on IO thread.
//
// Here's an example of a typical IPC dialog for video capture:
//
//   Renderer                          VideoCaptureHost
//      |                                     |
//      |  VideoCaptureHostMsg_Start >        |
//      |  < VideoCaptureMsg_DeviceInfo       |
//      |                                     |
//      | < VideoCaptureMsg_StateChanged      |
//      |        (kStarted)                   |
//      | < VideoCaptureMsg_BufferReady       |
//      |             ...                     |
//      | < VideoCaptureMsg_BufferReady       |
//      |             ...                     |
//      | VideoCaptureHostMsg_BufferReady >   |
//      | VideoCaptureHostMsg_BufferReady >   |
//      |                                     |
//      |             ...                     |
//      |                                     |
//      | < VideoCaptureMsg_BufferReady       |
//      |  VideoCaptureHostMsg_Stop >         |
//      | VideoCaptureHostMsg_BufferReady >   |
//      | < VideoCaptureMsg_StateChanged      |
//      |         (kStopped)                  |
//      v                                     v

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_HOST_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/renderer_host/video_capture_controller.h"
#include "ipc/ipc_message.h"

class VideoCaptureHost : public BrowserMessageFilter,
                         public VideoCaptureController::EventHandler {
 public:
  VideoCaptureHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing();
  virtual void OnDestruct() const;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  // VideoCaptureController::EventHandler implementation.
  virtual void OnError(VideoCaptureController::ControllerId id);
  virtual void OnBufferReady(VideoCaptureController::ControllerId id,
                             TransportDIB::Handle handle,
                             base::Time timestamp);
  virtual void OnFrameInfo(VideoCaptureController::ControllerId id,
                           int width,
                           int height,
                           int frame_per_second);
  virtual void OnReadyToDelete(VideoCaptureController::ControllerId id);

 private:
  friend class BrowserThread;
  friend class DeleteTask<VideoCaptureHost>;
  friend class MockVideoCaptureHost;
  friend class VideoCaptureHostTest;

  virtual ~VideoCaptureHost();

  // IPC message: Start capture on the VideoCaptureDevice referenced by
  // VideoCaptureParams::session_id. device_id is an id created by
  // VideCaptureMessageFilter to identify a session
  // between a VideCaptureMessageFilter and a VideoCaptureHost.
  void OnStartCapture(const IPC::Message& msg, int device_id,
                      const media::VideoCaptureParams& params);

  // IPC message: Stop capture on device referenced by device_id.
  void OnStopCapture(const IPC::Message& msg, int device_id);

  // IPC message: Pause capture on device referenced by device_id.
  void OnPauseCapture(const IPC::Message& msg, int device_id);

  // IPC message: Receive an empty buffer from renderer. Send it to device
  // referenced by |device_id|.
  void OnReceiveEmptyBuffer(const IPC::Message& msg,
                            int device_id,
                            TransportDIB::Handle handle);


  // Called on the IO thread when VideoCaptureController have
  // reported that all DIBs have been returned.
  void DoDeleteVideoCaptureController(VideoCaptureController::ControllerId id);

  // Send a filled buffer to the VideoCaptureMessageFilter.
  void DoSendFilledBuffer(int32 routing_id,
                          int device_id,
                          TransportDIB::Handle handle,
                          base::Time timestamp);

  // Send a information about frame resolution and frame rate
  // to the VideoCaptureMessageFilter.
  void DoSendFrameInfo(int32 routing_id,
                       int device_id,
                       int width,
                       int height,
                       int frame_per_second);

  // Handle error coming from VideoCaptureDevice.
  void DoHandleError(int32 routing_id, int device_id);

  typedef std::map<VideoCaptureController::ControllerId,
                   scoped_refptr<VideoCaptureController> >EntryMap;

  // A map of VideoCaptureController::ControllerId to VideoCaptureController
  // objects that is currently active.
  EntryMap entries_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_HOST_H_
