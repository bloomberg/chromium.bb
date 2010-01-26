// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_DEMOS_FRAMEWORK_WINDOW_H_
#define GPU_DEMOS_FRAMEWORK_WINDOW_H_

#include "base/scoped_ptr.h"
#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/platform.h"

namespace gpu {
namespace demos {

// Acts as a framework for standalone demos. It creates a window and delegates
// all events to demo to perform rendering and other tasks.
class Window {
 public:
  Window();
  virtual ~Window();

  // Initializes and creates a window with given dimensions.
  bool Init(int width, int height);

  // Enters the event processing loop.
  void MainLoop();

  void OnPaint();

 private:
  NativeWindowHandle window_handle_;
  scoped_ptr<Demo> demo_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_FRAMEWORK_WINDOW_H_
