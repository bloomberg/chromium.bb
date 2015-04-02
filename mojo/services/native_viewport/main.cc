// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/common/tracing_impl.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/gles2/gpu_impl.h"
#include "mojo/services/native_viewport/native_viewport_impl.h"
#include "third_party/mojo_services/src/native_viewport/public/cpp/args.h"
#include "ui/events/event_switches.h"
#include "ui/gl/gl_surface.h"

using mojo::ApplicationConnection;
using mojo::Gpu;
using mojo::NativeViewport;

namespace native_viewport {

class NativeViewportAppDelegate : public mojo::ApplicationDelegate,
                                  public mojo::InterfaceFactory<NativeViewport>,
                                  public mojo::InterfaceFactory<Gpu> {
 public:
  NativeViewportAppDelegate() : is_headless_(false) {}
  ~NativeViewportAppDelegate() override {}

 private:
  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* application) override {
    tracing_.Initialize(application);

#if defined(OS_LINUX)
    // Apply the switch for kTouchEvents to CommandLine (if set). This allows
    // redirecting the mouse to a touch device on X for testing.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const std::string touch_event_string("--" +
                                         std::string(switches::kTouchDevices));
    auto touch_iter = std::find(application->args().begin(),
                                application->args().end(),
                                touch_event_string);
    if (touch_iter != application->args().end() &&
        ++touch_iter != application->args().end()) {
      command_line->AppendSwitchASCII(touch_event_string, *touch_iter);
    }
#endif

    if (application->HasArg(mojo::kUseTestConfig))
      gfx::GLSurface::InitializeOneOffForTests();
    else
      gfx::GLSurface::InitializeOneOff();

    is_headless_ = application->HasArg(mojo::kUseHeadlessConfig);
  }

  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<NativeViewport>(this);
    connection->AddService<Gpu>(this);
    return true;
  }

  // mojo::InterfaceFactory<NativeViewport> implementation.
  void Create(ApplicationConnection* connection,
              mojo::InterfaceRequest<NativeViewport> request) override {
    if (!gpu_state_.get())
      gpu_state_ = new gles2::GpuState;
    new NativeViewportImpl(is_headless_, gpu_state_, request.Pass());
  }

  // mojo::InterfaceFactory<Gpu> implementation.
  void Create(ApplicationConnection* connection,
              mojo::InterfaceRequest<Gpu> request) override {
    if (!gpu_state_.get())
      gpu_state_ = new gles2::GpuState;
    new gles2::GpuImpl(request.Pass(), gpu_state_);
  }

  scoped_refptr<gles2::GpuState> gpu_state_;
  bool is_headless_;
  mojo::TracingImpl tracing_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportAppDelegate);
};
}

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(
      new native_viewport::NativeViewportAppDelegate);
  runner.set_message_loop_type(base::MessageLoop::TYPE_UI);
  return runner.Run(shell_handle);
}
