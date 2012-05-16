// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_SHADER_BENCH_PAINTER_H_
#define MEDIA_TOOLS_SHADER_BENCH_PAINTER_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"

// Class that paints video frames to a window.
class Painter {
 public:
  Painter();
  virtual ~Painter();

  // Loads frames into Painter. Painter does not take ownership of frames.
  virtual void LoadFrames(
      std::deque<scoped_refptr<media::VideoFrame> >* frames);

  // Called window is ready to be painted.
  virtual void OnPaint();

  // Initialize a Painter class with a width and a height
  virtual void Initialize(int width, int height) = 0;

  // Paint a single frame to a window.
  virtual void Paint(scoped_refptr<media::VideoFrame> video_frame) = 0;

 private:
  // Frames that the Painter will paint.
  std::deque<scoped_refptr<media::VideoFrame> >* frames_;

  DISALLOW_COPY_AND_ASSIGN(Painter);
};

#endif  // MEDIA_TOOLS_SHADER_BENCH_PAINTER_H_
