// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_SURFACE_CAPTURER_H_
#define CONTENT_COMMON_GPU_SURFACE_CAPTURER_H_

#include "content/common/content_export.h"
#include "media/base/video_frame.h"

namespace gfx {

class Size;
class Rect;

}  // namespace ui

namespace content {

// Surface capturer interface.  This interface is implemented by classes
// that perform image capturing from the backbuffer.
class CONTENT_EXPORT SurfaceCapturer {
 public:
  enum Error {
    // Invalid argument was passed to an API method.
    kInvalidArgumentError,
    // A failure occurred at the GPU process or one of its dependencies.
    // Examples of such failures include GPU hardware failures, GPU driver
    // failures, GPU library failures, GPU process programming errors, and so
    // on.
    kPlatformFailureError,
  };

  class CONTENT_EXPORT Client {
   public:
    // Callback to notify client of parameters of the backbuffer capture.  Every
    // time the Client receives this callback, subsequent media::VideoFrames
    // passed to CopyCaptureToVideoFrame() should mind the new parameters.
    // Parameters:
    //  |buffer_size| is the required logical size (in pixels) of the buffer
    //  to capture to (corresponds to |frame->coded_size()| in
    //  CopyCaptureToVideoFrame().
    //  |visible_rect| is the visible subrect of the actual screen capture
    //  contents in the buffer to capture to (corresponds to
    //  |frame->visible_rect()| in CopyCaptureToVideoFrame().
    virtual void NotifyCaptureParameters(const gfx::Size& buffer_size,
                                         const gfx::Rect& visible_rect) = 0;

    // Callback to notify client that CopyCaptureToVideoFrame() has been
    // completed for |frame|.  After this call, the capturer will drop all its
    // outstanding references to |frame|.
    // Parameters:
    //  |frame| is the completed copied captured frame.
    virtual void NotifyCopyCaptureDone(
        const scoped_refptr<media::VideoFrame>& frame) = 0;

    // Error notification callback.
    // Parameters:
    //  |error| is the error to report.
    virtual void NotifyError(Error error) = 0;

   protected:
    // Clients are not owned by Capturer instances and should not be deleted
    // through these pointers.
    virtual ~Client() {}
  };

  // Initialize the capturer to a specific configuration.
  // Parameters:
  //  |format| is the format to capture to (corresponds to |frame->format()| in
  //  CopyCaptureToVideoFrame()).  The NotifyCaptureParameters() callback is
  //  made to the Client on success; on failure, a NotifyError() callback is
  //  performed.
  virtual void Initialize(media::VideoFrame::Format format) = 0;

  // Attempt to capture a single frame.  This call is advisory to note to the
  // SurfaceCapturer that capture should be attempted at this time; success is
  // not guaranteed.  The most recent captured frame is cached internally, and
  // its contents returned every time CopyCaptureToVideoFrame() is called.
  virtual void TryCapture() = 0;

  // Copy the most recent captured contents to |frame|.
  // Parameters:
  //  |frame| is the media::VideoFrame to fill with captured contents.
  virtual void CopyCaptureToVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame) = 0;

  // Destroys the capturer; all pending inputs and outputs are dropped
  // immediately and the component is freed.  This call may asynchronously free
  // system resources, but its client-visible effects are synchronous.  After
  // this method returns no more callbacks will be made on the client.  Deletes
  // |this| unconditionally, so make sure to drop all pointers to it!
  virtual void Destroy() = 0;

  virtual ~SurfaceCapturer();
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_SURFACE_CAPTURER_H_
