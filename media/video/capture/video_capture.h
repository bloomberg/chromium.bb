// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains abstract classes used for media filter to handle video
// capture devices.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class VideoFrame;

class MEDIA_EXPORT VideoCapture {
 public:
  // TODO(wjia): add error codes.
  // TODO(wjia): support weak ptr.
  // Callbacks provided by client for notification of events.
  class MEDIA_EXPORT EventHandler {
   public:
    // Notify client that video capture has been started.
    virtual void OnStarted(VideoCapture* capture) = 0;

    // Notify client that video capture has been stopped.
    virtual void OnStopped(VideoCapture* capture) = 0;

    // Notify client that video capture has been paused.
    virtual void OnPaused(VideoCapture* capture) = 0;

    // Notify client that video capture has hit some error |error_code|.
    virtual void OnError(VideoCapture* capture, int error_code) = 0;

    // Notify client that the client has been removed and no more calls will be
    // received.
    virtual void OnRemoved(VideoCapture* capture) = 0;

    // Notify client that a buffer is available.
    virtual void OnFrameReady(
        VideoCapture* capture,
        const scoped_refptr<media::VideoFrame>& frame) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  typedef base::Callback<void(const media::VideoCaptureFormats&)>
      DeviceFormatsCallback;

  typedef base::Callback<void(const media::VideoCaptureFormats&)>
      DeviceFormatsInUseCallback;

  VideoCapture() {}

  // Request video capture to start capturing with |params|.
  // Also register |handler| with video capture for event handling.
  // |handler| must remain valid until it has received |OnRemoved()|.
  virtual void StartCapture(EventHandler* handler,
                            const VideoCaptureParams& params) = 0;

  // Request video capture to stop capturing for client |handler|.
  // |handler| must remain valid until it has received |OnRemoved()|.
  virtual void StopCapture(EventHandler* handler) = 0;

  virtual bool CaptureStarted() = 0;
  virtual int CaptureFrameRate() = 0;

  // Request the device capture supported formats. This method can be called
  // before startCapture() and/or after stopCapture() so a |callback| is used
  // instead of replying via EventHandler.
  virtual void GetDeviceSupportedFormats(
      const DeviceFormatsCallback& callback) = 0;

  // Request the device capture in-use format(s), possibly by other user(s) in
  // other renderer(s). If there is no format in use, the vector returned in
  // the callback will be empty.
  virtual void GetDeviceFormatsInUse(
      const DeviceFormatsInUseCallback& callback) = 0;

 protected:
  virtual ~VideoCapture() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCapture);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
