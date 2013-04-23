// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "media/base/media_export.h"
#include "media/video/capture/screen/shared_buffer.h"

namespace media {

class ScreenCaptureData;
struct MouseCursorShape;
class SharedBuffer;

// Class used to capture video frames asynchronously.
//
// The full capture sequence is as follows:
//
// (1) Start
//     This is when pre-capture steps are executed, such as flagging the
//     display to prevent it from sleeping during a session.
//
// (2) CaptureFrame
//     This is where the bits for the invalid rects are packaged up and sent
//     to the encoder.
//     A screen capture is performed if needed. For example, Windows requires
//     a capture to calculate the diff from the previous screen, whereas the
//     Mac version does not.
//
// Implementation has to ensure the following guarantees:
// 1. Double buffering
//    Since data can be read while another capture action is happening.
class MEDIA_EXPORT ScreenCapturer {
 public:
  // Provides callbacks used by the capturer to pass captured video frames and
  // mouse cursor shapes to the processing pipeline.
  class MEDIA_EXPORT Delegate {
   public:
    // Creates a shared memory buffer of the given size. Returns NULL if shared
    // buffers are not supported.
    virtual scoped_refptr<SharedBuffer> CreateSharedBuffer(uint32 size);

    // Notifies the delegate that the buffer is no longer used and can be
    // released.
    virtual void ReleaseSharedBuffer(scoped_refptr<SharedBuffer> buffer);

    // Called when a video frame has been captured. |capture_data| describes
    // a captured frame.
    virtual void OnCaptureCompleted(
        scoped_refptr<ScreenCaptureData> capture_data) = 0;

    // Called when the cursor shape has changed.
    // TODO(sergeyu): Move cursor shape capturing to a separate class.
    virtual void OnCursorShapeChanged(
        scoped_ptr<MouseCursorShape> cursor_shape) = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~ScreenCapturer() {}

  // Creates platform-specific capturer.
  static scoped_ptr<ScreenCapturer> Create();

#if defined(OS_LINUX)
  // Creates platform-specific capturer and instructs it whether it should use
  // X DAMAGE support.
  static scoped_ptr<ScreenCapturer> CreateWithXDamage(bool use_x_damage);
#elif defined(OS_WIN)
  // Creates Windows-specific capturer and instructs it whether or not to
  // disable desktop compositing.
  static scoped_ptr<ScreenCapturer> CreateWithDisableAero(bool disable_aero);
#endif

  // Called at the beginning of a capturing session. |delegate| must remain
  // valid until Stop() is called.
  virtual void Start(Delegate* delegate) = 0;

  // Captures the screen data associated with each of the accumulated
  // dirty region. When the capture is complete, the delegate is notified even
  // if the dirty region is empty.
  //
  // It is OK to call this method while another thread is reading
  // data of the previous capture. There can be at most one concurrent read
  // going on when this method is called.
  virtual void CaptureFrame() = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_H_
