// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/gles2/gpu_impl.h"
#include "components/native_viewport/native_viewport_impl.h"
#include "components/native_viewport/public/cpp/args.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/common/tracing_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
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

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    is_headless_ = command_line->HasSwitch(mojo::kUseHeadlessConfig);
    if (!is_headless_) {
      if (command_line->HasSwitch(mojo::kUseTestConfig))
        gfx::GLSurface::InitializeOneOffForTests();
      else
        gfx::GLSurface::InitializeOneOff();
    }
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
