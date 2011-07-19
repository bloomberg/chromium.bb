// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_SHADER_BENCH_WINDOW_H_
#define MEDIA_TOOLS_SHADER_BENCH_WINDOW_H_

#include "ui/gfx/native_widget_types.h"

class Painter;
class Task;

namespace media {

class Window {
 public:
  Window(int width, int height);

  // Creates and returns a handle to a native window of the given dimensions.
  gfx::NativeWindow CreateNativeWindow(int width, int height);

  // Returns the NPAPI plugin window handle of the window.
  gfx::PluginWindowHandle PluginWindow();

  // Kicks off frame painting with the given limit, painter, and
  // callback to run when painting task is complete.
  void Start(int limit, Task* done_task, Painter* painter);

  // Called when window is expected to paint self.
  void OnPaint();

  // Main loop for window.
  void MainLoop();

 private:
  // Task to run when frame painting is completed. Will be deleted after
  // running.
  Task* done_task_;

  // Reference to painter Window uses to paint frames.
  Painter* painter_;

  // Number of frames to paint before closing the window.
  int limit_;

  // Number of frames currently painted.
  int count_;

  // True if the window is painting video frames to the screen, false otherwise.
  bool running_;

  // This window's native handle.
  gfx::NativeWindow window_handle_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace media

#endif  // MEDIA_TOOLS_SHADER_BENCH_WINDOW_H_
