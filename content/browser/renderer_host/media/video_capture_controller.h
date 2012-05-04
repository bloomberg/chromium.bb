// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureController is the glue between VideoCaptureHost,
// VideoCaptureManager and VideoCaptureDevice.
// It provides functions for VideoCaptureHost to start a VideoCaptureDevice and
// is responsible for keeping track of shared DIBs and filling them with I420
// video frames for IPC communication between VideoCaptureHost and
// VideoCaptureMessageFilter.
// It implements media::VideoCaptureDevice::EventHandler to get video frames
// from a VideoCaptureDevice object and do color conversion straight into the
// shared DIBs to avoid a memory copy.
// It serves multiple VideoCaptureControllerEventHandlers.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "base/synchronization/lock.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/media/video_capture.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace media_stream {
class VideoCaptureManager;
}  // namespace media_stream

class VideoCaptureController
    : public base::RefCountedThreadSafe<VideoCaptureController>,
      public media::VideoCaptureDevice::EventHandler {
 public:
  VideoCaptureController(
      media_stream::VideoCaptureManager* video_capture_manager);

  // Start video capturing and try to use the resolution specified in
  // |params|.
  // When capturing has started, the |event_handler| receives a call OnFrameInfo
  // with resolution that best matches the requested that the video
  // capture device support.
  void StartCapture(const VideoCaptureControllerID& id,
                    VideoCaptureControllerEventHandler* event_handler,
                    base::ProcessHandle render_process,
                    const media::VideoCaptureParams& params);

  // Stop video capture.
  // When the capture is stopped and all DIBs have been returned,
  // VideoCaptureControllerEventHandler::OnReadyToDelete will be called.
  // |force_buffer_return| allows controller to take back all buffers used
  // by |event_handler|.
  void StopCapture(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* event_handler,
                   bool force_buffer_return);

  // API called directly by VideoCaptureManager in case the device is
  // prematurely closed.
  void StopSession(int session_id);

  // Return a buffer previously given in
  // VideoCaptureControllerEventHandler::OnBufferReady.
  void ReturnBuffer(const VideoCaptureControllerID& id,
                    VideoCaptureControllerEventHandler* event_handler,
                    int buffer_id);

  // Implement media::VideoCaptureDevice::EventHandler.
  virtual void OnIncomingCapturedFrame(const uint8* data,
                                       int length,
                                       base::Time timestamp) OVERRIDE;
  virtual void OnError() OVERRIDE;
  virtual void OnFrameInfo(
      const media::VideoCaptureCapability& info) OVERRIDE;

 protected:
  virtual ~VideoCaptureController();

 private:
  friend class base::RefCountedThreadSafe<VideoCaptureController>;

  struct ControllerClient;
  typedef std::list<ControllerClient*> ControllerClients;

  // Callback when manager has stopped device.
  void OnDeviceStopped();

  // Worker functions on IO thread.
  void DoIncomingCapturedFrameOnIOThread(int buffer_id, base::Time timestamp);
  void DoFrameInfoOnIOThread(const media::VideoCaptureCapability info);
  void DoErrorOnIOThread();
  void DoDeviceStoppedOnIOThread();

  // Send frame info and init buffers to |client|.
  void SendFrameInfoAndBuffers(ControllerClient* client, int buffer_size);

  // Helpers.
  // Find a client of |id| and |handler| in |clients|.
  ControllerClient* FindClient(
      const VideoCaptureControllerID& id,
      VideoCaptureControllerEventHandler* handler,
      const ControllerClients& clients);
  // Find a client of |session_id| in |clients|.
  ControllerClient* FindClient(
      int session_id,
      const ControllerClients& clients);
  // Decide what to do after kStopping state. Dependent on events, controller
  // can stay in kStopping state, or go to kStopped, or restart capture.
  void PostStopping();
  // Check if any DIB is used by client.
  bool ClientHasDIB();

  // Lock to protect free_dibs_ and owned_dibs_.
  base::Lock lock_;

  struct SharedDIB;
  typedef std::map<int /*buffer_id*/, SharedDIB*> DIBMap;
  // All DIBs created by this object.
  // It's modified only on IO thread.
  DIBMap owned_dibs_;

  // All clients served by this controller.
  ControllerClients controller_clients_;

  // All clients waiting for service.
  ControllerClients pending_clients_;

  // The parameter that currently used for the capturing.
  media::VideoCaptureParams current_params_;

  // It's modified on caller thread, assuming there is only one OnFrameInfo()
  // call per StartCapture().
  media::VideoCaptureCapability frame_info_;

  // It's accessed only on IO thread.
  bool frame_info_available_;

  media_stream::VideoCaptureManager* video_capture_manager_;

  bool device_in_use_;
  video_capture::State state_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureController);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
