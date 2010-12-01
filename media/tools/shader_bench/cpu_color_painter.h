// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_SHADER_BENCH_CPU_COLOR_PAINTER_H_
#define MEDIA_TOOLS_SHADER_BENCH_CPU_COLOR_PAINTER_H_

#include "base/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/tools/shader_bench/gpu_painter.h"

// Does color conversion using CPU, rendering on GPU.
class CPUColorPainter : public GPUPainter {
 public:
  CPUColorPainter();
  virtual ~CPUColorPainter();

  // Painter interface.
  virtual void Initialize(int width, int height);
  virtual void Paint(scoped_refptr<media::VideoFrame> video_frame);

 private:
  // Shader program id.
  GLuint program_id_;

  // ID of rgba texture.
  GLuint textures_[1];

  DISALLOW_COPY_AND_ASSIGN(CPUColorPainter);
};

#endif  // MEDIA_TOOLS_SHADER_BENCH_CPU_COLOR_PAINTER_H_
