// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
#define MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_

#include <X11/Xlib.h>

#include "base/basictypes.h"

class MessageLoop;

namespace media {
class VideoFrame;
}

class X11VideoRenderer {
 public:
  X11VideoRenderer(Display* display, Window window);
  ~X11VideoRenderer();

  void Paint(media::VideoFrame* video_frame);

 private:
  // Initializes X11 rendering for the given dimensions.
  void Initialize(int width, int height);

  Display* display_;
  Window window_;

  // Image in heap that contains the RGBA data of the video frame.
  XImage* image_;

  // Picture represents the paint target. This is a picture located
  // in the server.
  unsigned long picture_;

  bool use_render_;

  DISALLOW_COPY_AND_ASSIGN(X11VideoRenderer);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_X11_VIDEO_RENDERER_H_
