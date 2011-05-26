// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureController is the glue between VideoCaptureHost,
// VideoCaptureManager and VideoCaptureDevice.
// It provides functions for VideoCaptureHost to start a VideoCaptureDevice and
// is responsible for keeping track of TransportDIBs and filling them with I420
// video frames for IPC communication between VideoCaptureHost and
// VideoCaptureMessageFilter.
// It implements media::VideoCaptureDevice::EventHandler to get video frames
// from a VideoCaptureDevice object and do color conversion straight into the
// TransportDIBs to avoid a memory copy.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>
#include <map>

#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "ui/gfx/surface/transport_dib.h"

class VideoCaptureController
    : public base::RefCountedThreadSafe<VideoCaptureController>,
      public media::VideoCaptureDevice::EventHandler {
 public:
  // Id used for identifying an object of VideoCaptureController.
  typedef std::pair<int32, int> ControllerId;
  class EventHandler {
   public:
    // An Error have occurred in the VideoCaptureDevice.
    virtual void OnError(ControllerId id) = 0;

    // An TransportDIB have been filled with I420 video.
    virtual void OnBufferReady(ControllerId id,
                               TransportDIB::Handle handle,
                               base::Time timestamp) = 0;

    // The frame resolution the VideoCaptureDevice capture video in.
    virtual void OnFrameInfo(ControllerId id,
                             int width,
                             int height,
                             int frame_rate) = 0;

    // Report that this object can be deleted.
    virtual void OnReadyToDelete(ControllerId id) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  VideoCaptureController(ControllerId id,
                         base::ProcessHandle render_process,
                         EventHandler* event_handler);
  virtual ~VideoCaptureController();

  // Starts video capturing and tries to use the resolution specified in
  // params.
  // When capturing has started EventHandler::OnFrameInfo is called with
  // resolution that best matches the requested that the video capture device
  // support.
  void StartCapture(const media::VideoCaptureParams& params);

  // Stop video capture.
  // When the capture is stopped and all TransportDIBS have been returned
  // EventHandler::OnReadyToDelete will be called.
  // stopped_task may be null but it can be used to get a notification when the
  // device is stopped.
  void StopCapture(Task* stopped_task);

  // Return a DIB previously given in EventHandler::OnBufferReady.
  void ReturnTransportDIB(TransportDIB::Handle handle);

  // Implement media::VideoCaptureDevice::EventHandler.
  virtual void OnIncomingCapturedFrame(const uint8* data,
                                       int length,
                                       base::Time timestamp);
  virtual void OnError();
  virtual void OnFrameInfo(const media::VideoCaptureDevice::Capability& info);

 private:
  // Called by VideoCaptureManager when a device have been stopped.
  void OnDeviceStopped(Task* stopped_task);

  // Lock to protect free_dibs_ and owned_dibs_.
  base::Lock lock_;
  // Handle to the render process that will receive the DIBs.
  base::ProcessHandle render_handle_;
  bool report_ready_to_delete_;
  typedef std::list<TransportDIB::Handle> DIBHandleList;
  typedef std::map<TransportDIB::Handle, TransportDIB*> DIBMap;

  // Free DIBS that can be filled with video frames.
  DIBHandleList free_dibs_;

  // All DIBS created by this object.
  DIBMap owned_dibs_;
  EventHandler* event_handler_;

  // The parameter that was requested when starting the capture device.
  media::VideoCaptureParams params_;

  // Id used for identifying this object.
  ControllerId id_;
  media::VideoCaptureDevice::Capability frame_info_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureController);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_CONTROLLER_H_
