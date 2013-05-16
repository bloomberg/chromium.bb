// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SHARED_DESKTOP_FRAME_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SHARED_DESKTOP_FRAME_H_

#include "base/memory/ref_counted.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace media {

// SharedDesktopFrame is a DesktopFrame that may have multiple instances all
// sharing the same buffer.
class SharedDesktopFrame : public webrtc::DesktopFrame {
 public:
  virtual ~SharedDesktopFrame();

  static SharedDesktopFrame* Wrap(webrtc::DesktopFrame* desktop_frame);

  // Returns the underlying instance of DesktopFrame.
  webrtc::DesktopFrame* GetUnderlyingFrame();

  // Creates a clone of this object.
  SharedDesktopFrame* Share();

  // Checks if the frame is currently shared. If it returns false it's
  // guaranteed that there are no clones of the object.
  bool IsShared();

 private:
  class Core;

  SharedDesktopFrame(scoped_refptr<Core> core);

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(SharedDesktopFrame);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SHARED_DESKTOP_FRAME_H_
