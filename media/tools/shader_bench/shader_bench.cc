// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <deque>
#include <ostream>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "media/base/video_frame.h"
#include "media/tools/shader_bench/cpu_color_painter.h"
#include "media/tools/shader_bench/gpu_color_painter.h"
#include "media/tools/shader_bench/painter.h"
#include "media/tools/shader_bench/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

static const int kNumFramesToPaint = 500;
static base::TimeTicks g_start_;
static base::TimeTicks g_end_;

long CalculateYUVFrameSize(FILE* file_handle, int num_frames) {
  fseek(file_handle, 0, SEEK_END);
  long file_size = (long) ftell(file_handle);
  rewind(file_handle);
  return file_size / num_frames;
}

void GetFrames(std::string file_name,
               int width, int height, int num_frames,
               std::deque<scoped_refptr<media::VideoFrame> >& out_frames) {
  FILE* file_handle = fopen(file_name.c_str(), "rb");
  if (!file_handle) {
    printf("Could not open %s\n", file_name.c_str());
    exit(1);
  }

  long frame_size = CalculateYUVFrameSize(file_handle, num_frames);

  for (int i = 0; i < num_frames; i++) {
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                       width,
                                       height,
                                       base::TimeDelta(),
                                       base::TimeDelta());
    long bytes_read =
        fread(video_frame->data(0), 1, frame_size, file_handle);

    if (bytes_read != frame_size) {
      printf("Could not read %s\n", file_name.c_str());
      fclose(file_handle);
      exit(1);
    }
    out_frames.push_back(video_frame);
  }

  fclose(file_handle);
}

void TestFinished() {
  g_end_ = base::TimeTicks::HighResNow();
  double time_in_seconds =
      static_cast<double>((g_end_ - g_start_).InMilliseconds()) / 1000;
  double fps = kNumFramesToPaint / time_in_seconds;
  printf("Printed %f frames per second.\n", fps);
}

void RunTest(media::Window* window, Painter* painter) {
  g_start_ = base::TimeTicks::HighResNow();
  window->Start(kNumFramesToPaint, base::Bind(&TestFinished), painter);
}

int main(int argc, char** argv) {
  // Read arguments.
  if (argc == 1) {
    printf("Usage: %s --file=FILE --wxh=DIMENSIONS --frames=NUM_FRAMES\n"
           "FILE is a raw .yuv file with 1+ frames in it\n"
           "DIMENSIONS is the width and height of the frame in pixels\n"
           "NUM_FRAMES is the number of frames in FILE\n", argv[0]);
    return 1;
  }

  // Read command line.
#if defined(TOOLKIT_GTK)
  gtk_init(&argc, &argv);
#endif
  CommandLine::Init(argc, argv);

  // Determine file name.
  std::string file_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("file");

  // Determine number of frames.
  int num_frames = 0;
  std::string str_num_frames =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("frames");
  base::StringToInt(str_num_frames, &num_frames);

  // Determine video dimensions.
  int width = 0;
  int height = 0;
  std::string dimensions =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII("wxh");
  int x_index = dimensions.find('x');
  std::string str_width = dimensions.substr(0, x_index);
  std::string str_height =
      dimensions.substr(x_index + 1, dimensions.length() - x_index - 1);
  base::StringToInt(str_width, &width);
  base::StringToInt(str_height, &height);

  // Process files.
  std::deque<scoped_refptr<media::VideoFrame> > frames;
  GetFrames(file_name, width, height, num_frames, frames);

  // Initialize window and graphics context.
  base::AtExitManager at_exit_manager;
  gfx::GLSurface::InitializeOneOff();
  scoped_ptr<media::Window> window(new media::Window(width, height));
  gfx::GLSurface* surface =
      gfx::GLSurface::CreateViewGLSurface(false, window->PluginWindow());
  gfx::GLContext* context = gfx::GLContext::CreateGLContext(
      NULL,
      surface,
      gfx::PreferDiscreteGpu);
  context->MakeCurrent(surface);
  // This sets D3DPRESENT_INTERVAL_IMMEDIATE on Windows.
  context->SetSwapInterval(0);

  // Initialize and name GPU painters.
  static const int kNumPainters = 3;
  static const struct {
    const char* name;
    GPUPainter* painter;
  } painters[] = {
    { "CPU CSC + GPU Render", new CPUColorPainter() },
    { "GPU CSC/Render", new GPUColorWithLuminancePainter() },
  };

  // Run GPU painter tests.
  for (int i = 0; i < kNumPainters; i++) {
    scoped_ptr<GPUPainter> painter(painters[i].painter);
    painter->LoadFrames(&frames);
    painter->SetGLContext(surface, context);
    painter->Initialize(width, height);
    printf("Running %s tests...", painters[i].name);
    RunTest(window.get(), painter.get());
  }

  return 0;
}
