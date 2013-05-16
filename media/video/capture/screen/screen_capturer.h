// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace media {

struct MouseCursorShape;

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
class MEDIA_EXPORT ScreenCapturer
    : public NON_EXPORTED_BASE(webrtc::DesktopCapturer) {
 public:
  // Provides callbacks used by the capturer to pass captured video frames and
  // mouse cursor shapes to the processing pipeline.
  //
  // TODO(sergeyu): Move cursor shape capturing to a separate class because it's
  // unrelated.
  class MEDIA_EXPORT MouseShapeObserver {
   public:
    // Called when the cursor shape has changed.
    virtual void OnCursorShapeChanged(
        scoped_ptr<MouseCursorShape> cursor_shape) = 0;

   protected:
    virtual ~MouseShapeObserver() {}
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
#endif  // defined(OS_WIN)

  // Called at the beginning of a capturing session. |mouse_shape_observer| must
  // remain valid until the capturer is destroyed.
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_H_
