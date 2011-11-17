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

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

namespace content {
class ResourceContext;
}  // namespace content

class CONTENT_EXPORT VideoCaptureHost
    : public BrowserMessageFilter,
      public VideoCaptureControllerEventHandler {
 public:
  explicit VideoCaptureHost(const content::ResourceContext* resource_context);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // VideoCaptureControllerEventHandler implementation.
  virtual void OnError(const VideoCaptureControllerID& id) OVERRIDE;
  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE;
  virtual void OnBufferReady(const VideoCaptureControllerID& id,
                             int buffer_id,
                             base::Time timestamp) OVERRIDE;
  virtual void OnFrameInfo(const VideoCaptureControllerID& id,
                           int width,
                           int height,
                           int frame_per_second) OVERRIDE;
  virtual void OnReadyToDelete(const VideoCaptureControllerID& id) OVERRIDE;

 private:
  friend class content::BrowserThread;
  friend class DeleteTask<VideoCaptureHost>;
  friend class MockVideoCaptureHost;
  friend class VideoCaptureHostTest;

  virtual ~VideoCaptureHost();

  // IPC message: Start capture on the VideoCaptureDevice referenced by
  // VideoCaptureParams::session_id. |device_id| is an id created by
  // VideCaptureMessageFilter to identify a session
  // between a VideCaptureMessageFilter and a VideoCaptureHost.
  void OnStartCapture(int device_id,
                      const media::VideoCaptureParams& params);
  void OnControllerAdded(
      int device_id, const media::VideoCaptureParams& params,
      VideoCaptureController* controller);
  void DoControllerAddedOnIOThreead(
      int device_id, const media::VideoCaptureParams params,
      VideoCaptureController* controller);

  // IPC message: Stop capture on device referenced by |device_id|.
  void OnStopCapture(int device_id);

  // IPC message: Pause capture on device referenced by |device_id|.
  void OnPauseCapture(int device_id);

  // IPC message: Receive an empty buffer from renderer. Send it to device
  // referenced by |device_id|.
  void OnReceiveEmptyBuffer(int device_id, int buffer_id);


  // Called on the IO thread when VideoCaptureController have
  // reported that all DIBs have been returned.
  void DoDeleteVideoCaptureController(const VideoCaptureControllerID& id);

  // Send a newly created buffer to the VideoCaptureMessageFilter.
  void DoSendNewBuffer(int device_id,
                       base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id);

  // Send a filled buffer to the VideoCaptureMessageFilter.
  void DoSendFilledBuffer(int device_id,
                          int buffer_id,
                          base::Time timestamp);

  // Send a information about frame resolution and frame rate
  // to the VideoCaptureMessageFilter.
  void DoSendFrameInfo(int device_id,
                       int width,
                       int height,
                       int frame_per_second);

  // Handle error coming from VideoCaptureDevice.
  void DoHandleError(int device_id);

  // Helpers.
  media_stream::VideoCaptureManager* GetVideoCaptureManager();

  typedef std::map<VideoCaptureControllerID,
                   scoped_refptr<VideoCaptureController> > EntryMap;
  // A map of VideoCaptureControllerID to VideoCaptureController
  // objects that is currently active.
  EntryMap entries_;

  typedef std::map<VideoCaptureControllerID,
                   media::VideoCapture::State> EntryState;
  // Record state of each VideoCaptureControllerID.
  EntryState entry_state_;

  // Used to get a pointer to VideoCaptureManager to start/stop capture devices.
  const content::ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
