// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureHost serves video capture related messages from
// VideoCaptureMessageFilter which lives inside the render process.
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
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_message.h"

class CONTENT_EXPORT VideoCaptureHost
    : public content::BrowserMessageFilter,
      public VideoCaptureControllerEventHandler {
 public:
  VideoCaptureHost();

  // content::BrowserMessageFilter implementation.
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
  virtual void OnPaused(const VideoCaptureControllerID& id) OVERRIDE;

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<VideoCaptureHost>;
  friend class MockVideoCaptureHost;
  friend class VideoCaptureHostTest;

  virtual ~VideoCaptureHost();

  // IPC message: Start capture on the VideoCaptureDevice referenced by
  // VideoCaptureParams::session_id. |device_id| is an id created by
  // VideoCaptureMessageFilter to identify a session
  // between a VideoCaptureMessageFilter and a VideoCaptureHost.
  void OnStartCapture(int device_id,
                      const media::VideoCaptureParams& params);
  void OnControllerAdded(
      int device_id, const media::VideoCaptureParams& params,
      VideoCaptureController* controller);
  void DoControllerAddedOnIOThread(
      int device_id, const media::VideoCaptureParams params,
      VideoCaptureController* controller);

  // IPC message: Stop capture on device referenced by |device_id|.
  void OnStopCapture(int device_id);

  // IPC message: Pause capture on device referenced by |device_id|.
  void OnPauseCapture(int device_id);

  // IPC message: Receive an empty buffer from renderer. Send it to device
  // referenced by |device_id|.
  void OnReceiveEmptyBuffer(int device_id, int buffer_id);

  // Send a newly created buffer to the VideoCaptureMessageFilter.
  void DoSendNewBufferOnIOThread(
      const VideoCaptureControllerID& controller_id,
      base::SharedMemoryHandle handle,
      int length,
      int buffer_id);

  // Send a filled buffer to the VideoCaptureMessageFilter.
  void DoSendFilledBufferOnIOThread(
      const VideoCaptureControllerID& controller_id,
      int buffer_id,
      base::Time timestamp);

  // Send information about frame resolution and frame rate
  // to the VideoCaptureMessageFilter.
  void DoSendFrameInfoOnIOThread(
      const VideoCaptureControllerID& controller_id,
      int width,
      int height,
      int frame_per_second);

  // Handle error coming from VideoCaptureDevice.
  void DoHandleErrorOnIOThread(const VideoCaptureControllerID& controller_id);

  void DoPausedOnIOThread(const VideoCaptureControllerID& controller_id);

  void DeleteVideoCaptureControllerOnIOThread(
      const VideoCaptureControllerID& controller_id);

  // Returns the video capture manager. This is a virtual function so that
  // the unit tests can inject their own MediaStreamManager.
  virtual media_stream::VideoCaptureManager* GetVideoCaptureManager();

  struct Entry;
  typedef std::map<VideoCaptureControllerID, Entry*> EntryMap;
  // A map of VideoCaptureControllerID to its state and VideoCaptureController.
  EntryMap entries_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
