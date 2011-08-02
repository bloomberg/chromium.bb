// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class for gles2 demos.

#ifndef GPU_DEMOS_FRAMEWORK_DEMO_H_
#define GPU_DEMOS_FRAMEWORK_DEMO_H_

#include <ctime>

namespace gpu {
namespace demos {

// Base class for GLES2 demos. The same demo class is supposed to be run as
// standalone apps (exe), pepper plugin (dll), and nacl module (nexe). This is
// accomplished by creating framework for each platfom and hosting the demo
// class object.
class Demo {
 public:
  Demo();
  virtual ~Demo();

  // Returns the title of demo. This title is used to name the window or
  // html page where demo is running.
  virtual const wchar_t* Title() const = 0;

  // This function is called by the framework to initialize the OpenGL state
  // required by this demo. When this function is called, it is assumed that
  // a rendering context has already been created and made current.
  virtual bool InitGL() = 0;

  // Returns whether the demo is animated. Animated demos are drawn
  // continuously. Unanimated demos are only drawn when the window is invalid.
  virtual bool IsAnimated();

  // This function is called by the network to notify demo that window
  // surface has been resized.
  void Resize(int width, int height);

  // This function is called by the framework to perform OpenGL rendering.
  // When this function is called, it is assumed that the rendering context
  // has been made current.
  void Draw();

 protected:
  // Returns the width of window.
  int width() const { return width_; }
  // Returns the height of window.
  int height() const { return height_; }

  // The framework calls this function for the derived classes to do custom
  // rendering. There is no default implementation. It must be defined by the
  // derived classes. The elapsed_sec param represents the time elapsed
  // (in seconds) after Render was called the last time. It can be used to
  // make the application frame-rate independent. It is 0.0f for the
  // first render call.
  virtual void Render(float elapsed_sec) = 0;

 private:
  int width_;  // Window width.
  int height_;  // Window height.

  // Time at which draw was called last.
  clock_t last_draw_time_;
};

}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_FRAMEWORK_DEMO_H_
