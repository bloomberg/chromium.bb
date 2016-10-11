// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureHost serves video capture related messages from
// VideoCaptureMessageFilter which lives inside the render process.
//
// This class is owned by RenderProcessHostImpl, and instantiated on UI
// thread, but all other operations and method calls happen on IO thread.
//
// Here's an example of a typical IPC dialog for video capture:
//
//   Renderer                             VideoCaptureHost
//      |                                        |
//      |  --------- StartCapture -------->      |
//      | <------ VideoCaptureObserver   ------  |
//      |         ::StateChanged(STARTED)        |
//      | < VideoCaptureMsg_NewBuffer(1)         |
//      | < VideoCaptureMsg_NewBuffer(2)         |
//      | < VideoCaptureMsg_NewBuffer(3)         |
//      |                                        |
//      | <-------- OnBufferReady(1) ---------   |
//      | <-------- OnBufferReady(2) ---------   |
//      | -------- ReleaseBuffer(1) --------->   |
//      | <-------- OnBufferReady(3) ---------   |
//      | -------- ReleaseBuffer(2) --------->   |
//      | <-------- OnBufferReady(1) ---------   |
//      | -------- ReleaseBuffer(3) --------->   |
//      | <-------- OnBufferReady(2) ---------   |
//      | -------- ReleaseBuffer(1) --------->   |
//      |             ...                        |
//      | <-------- OnBufferReady(3) ---------   |
//      =                                        =
//      |             ... (resolution change)    |
//      | <------ OnBufferDestroyed(3) -------   |  Buffers are re-allocated
//      | < VideoCaptureMsg_NewBuffer(4)         |  with a larger size, as
//      | <-------- OnBufferReady(4) ---------   |  needed.
//      | -------- ReleaseBuffer(2) --------->   |
//      | <------ OnBufferDestroyed(2) -------   |
//      | < VideoCaptureMsg_NewBuffer(5)         |
//      | <-------- OnBufferReady(5) ---------   |
//      =             ...                        =
//      |                                        |
//      | < VideoCaptureMsg_BufferReady          |
//      |  --------- StopCapture --------->      |
//      | -------- ReleaseBuffer(n) --------->   |
//      | <------ VideoCaptureObserver   ------  |
//      |         ::StateChanged(STOPPED)        |
//      v                                        v

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/video_capture.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_message.h"

namespace content {
class MediaStreamManager;

class CONTENT_EXPORT VideoCaptureHost
    : public BrowserMessageFilter,
      public VideoCaptureControllerEventHandler,
      public BrowserAssociatedInterface<mojom::VideoCaptureHost>,
      public mojom::VideoCaptureHost {
 public:
  explicit VideoCaptureHost(MediaStreamManager* media_stream_manager);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // VideoCaptureControllerEventHandler implementation.
  void OnError(VideoCaptureControllerID id) override;
  void OnBufferCreated(VideoCaptureControllerID id,
                       base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id) override;
  void OnBufferDestroyed(VideoCaptureControllerID id,
                         int buffer_id) override;
  void OnBufferReady(VideoCaptureControllerID id,
                     int buffer_id,
                     const scoped_refptr<media::VideoFrame>& frame) override;
  void OnEnded(VideoCaptureControllerID id) override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<VideoCaptureHost>;
  friend class MockVideoCaptureHost;
  friend class VideoCaptureHostTest;

  void DoError(VideoCaptureControllerID id);
  void DoEnded(VideoCaptureControllerID id);

  ~VideoCaptureHost() override;

  // mojom::VideoCaptureHost implementation
  void Start(int32_t device_id,
             int32_t session_id,
             const media::VideoCaptureParams& params,
             mojom::VideoCaptureObserverPtr observer) override;
  void Stop(int32_t device_id) override;
  void Pause(int32_t device_id) override;
  void Resume(int32_t device_id,
              int32_t session_id,
              const media::VideoCaptureParams& params) override;
  void RequestRefreshFrame(int32_t device_id) override;
  void ReleaseBuffer(int32_t device_id,
                     int32_t buffer_id,
                     const gpu::SyncToken& sync_token,
                     double consumer_resource_utilization) override;
  void GetDeviceSupportedFormats(
      int32_t device_id,
      int32_t session_id,
      const GetDeviceSupportedFormatsCallback& callback) override;
  void GetDeviceFormatsInUse(
      int32_t device_id,
      int32_t session_id,
      const GetDeviceFormatsInUseCallback& callback) override;

  void OnControllerAdded(
      int device_id,
      const base::WeakPtr<VideoCaptureController>& controller);

  // Deletes the controller and notifies the VideoCaptureManager. |on_error| is
  // true if this is triggered by VideoCaptureControllerEventHandler::OnError.
  void DeleteVideoCaptureController(VideoCaptureControllerID controller_id,
                                    bool on_error);

  MediaStreamManager* const media_stream_manager_;

  // A map of VideoCaptureControllerID to the VideoCaptureController to which it
  // is connected. An entry in this map holds a null controller while it is in
  // the process of starting.
  std::map<VideoCaptureControllerID, base::WeakPtr<VideoCaptureController>>
      controllers_;

  // VideoCaptureObservers map, each one is used and should be valid between
  // Start() and the corresponding Stop().
  std::map<int32_t, mojom::VideoCaptureObserverPtr> device_id_to_observer_map_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_HOST_H_
